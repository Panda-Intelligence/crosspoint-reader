/*
 * GDEQ0426T82 / SSD1677 virtual e-ink panel for the Mofei simulator.
 *
 * Implements the Hybrid FSM design from
 *   .trellis/tasks/05-04-mofei-simulator-bringup/research/eink-spi-modeling.md
 * Approach C: faithful command + address-window state machine, skip-and-consume
 * for LUT bytes / softstart / border / temperature, single 800×480 1bpp
 * framebuffer pushed to the host UI on Master Activation (cmd 0x20).
 *
 * Wire protocol to the host UI: see
 *   .trellis/tasks/05-04-mofei-simulator-bringup/research/qemu-peripheral-ipc.md
 * 8-byte header (channel:u8, flags:u8, reserved:u16, payload_len:u32) +
 * payload, multiplexed on a single chardev backend. This peripheral writes
 * channel 0x01 (framebuffer).
 *
 * Integration status (PR2):
 *   - This file is committed against the espressif/qemu fork's hw/display/
 *     layout, but is NOT yet wired into the fork's Meson build. The build
 *     wiring lands when verification-pending.md item #4 (machine flag) is
 *     confirmed and we know the exact ESP32-S3 SSI bus surface we attach to.
 *   - Compile-checked status: NOT YET. This file will fail to build until
 *     QEMU headers are visible. See verification-pending.md.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ssi/ssi.h"
#include "hw/irq.h"
#include "chardev/char-fe.h"
#include "migration/vmstate.h"

#define TYPE_SSD1677_GDEQ0426 "ssd1677-gdeq0426"
OBJECT_DECLARE_SIMPLE_TYPE(SSD1677State, SSD1677_GDEQ0426)

#define EPD_WIDTH               800
#define EPD_HEIGHT              480
#define EPD_FB_BYTES            ((EPD_WIDTH * EPD_HEIGHT) / 8)  /* 48000 */
#define EPD_ROW_STRIDE_BYTES    (EPD_WIDTH / 8)                  /* 100 */

/* IPC channel tags — keep in sync with simulator/src-tauri/src/ipc.rs */
#define IPC_CHANNEL_FRAMEBUFFER 0x01

/* Frame flags */
#define IPC_FLAG_FB_FULL        0x00
#define IPC_FLAG_FB_PARTIAL     0x01

/* SSD1677 commands we model. See research/eink-spi-modeling.md table for the
 * MUST/SHOULD/MAY classification. */
#define CMD_DRIVER_OUTPUT_CTL    0x01
#define CMD_GATE_VOLTAGE         0x03  /* MAY — consume args, no-op */
#define CMD_SOURCE_VOLTAGE       0x04
#define CMD_SOFTSTART            0x0C
#define CMD_DEEP_SLEEP           0x10
#define CMD_DATA_ENTRY_MODE      0x11
#define CMD_SW_RESET             0x12
#define CMD_TEMPERATURE_SENSOR   0x18
#define CMD_MASTER_ACTIVATION    0x20
#define CMD_DISPLAY_UPDATE_CTL_1 0x21
#define CMD_DISPLAY_UPDATE_CTL_2 0x22
#define CMD_WRITE_RAM_BW         0x24
#define CMD_WRITE_RAM_RED        0x26
#define CMD_VCOM_CTL             0x2C
#define CMD_WRITE_LUT            0x32
#define CMD_BORDER_WAVEFORM      0x3C
#define CMD_RAM_X_START_END      0x44
#define CMD_RAM_Y_START_END      0x45
#define CMD_AUTO_WRITE_RED       0x46
#define CMD_AUTO_WRITE_BW        0x47
#define CMD_RAM_X_COUNTER        0x4E
#define CMD_RAM_Y_COUNTER        0x4F

/* Refresh delays (ms) — scaled down from real timings (3.5s/1.5s/0.42s).
 * Override with env var MOFEI_SIM_FAST_REFRESH=1 to skip entirely. */
#define REFRESH_MS_FULL          500
#define REFRESH_MS_FAST          200
#define REFRESH_MS_PARTIAL       50

typedef enum {
    PARSE_IDLE,                 /* expecting next command byte */
    PARSE_COLLECTING_ARGS,      /* mid-command, awaiting fixed arg count */
    PARSE_RAM_WRITE,            /* inside 0x24/0x26 RAM write — until next CS rise or new cmd */
    PARSE_LUT_CONSUME,          /* inside 0x32 — count down LUT byte budget */
} ParserMode;

struct SSD1677State {
    SSIPeripheral parent_obj;

    CharBackend chr;            /* host IPC chardev */

    /* Pin inputs (driven by guest via GPIO matrix → qemu_irq sinks here) */
    bool dc_high;               /* D/C# state — high = data, low = command */
    bool reset_low;              /* RES# state — low = held in reset */

    /* Pin outputs */
    qemu_irq busy_out;           /* BUSY pin to guest */

    /* Parser state */
    uint8_t mode;  /* ParserMode, stored as uint8_t for VMSTATE_UINT8 */
    uint8_t current_cmd;
    uint8_t arg_buf[8];
    uint8_t arg_index;
    uint8_t arg_expected;
    uint16_t lut_consume_remaining;

    /* Command-driven state */
    bool deep_sleep;
    bool busy_high;              /* mirror of busy_out for vmstate */

    /* Address window + counters */
    uint16_t ram_x_start, ram_x_end;     /* in 8-pixel units */
    uint16_t ram_y_start, ram_y_end;     /* in pixel rows */
    uint16_t ram_x_cur;
    uint16_t ram_y_cur;
    uint8_t  data_entry_mode;            /* command 0x11 byte */

    /* Display update control */
    uint8_t  display_update_ctl_2;       /* command 0x22 byte */

    /* Framebuffers — single 48KB B/W buffer; "red"/"old" buffer ignored for B/W panel */
    uint8_t  fb[EPD_FB_BYTES];
    uint8_t  fb_red[EPD_FB_BYTES];       /* secondary buffer for cmd 0x26 */

    /* Refresh-delay timer */
    QEMUTimer *busy_timer;
};

/* ---------------------------- Host IPC ----------------------------------- */

static void ipc_send_frame(SSD1677State *s, uint8_t channel, uint8_t flags,
                           const uint8_t *payload, uint32_t len)
{
    uint8_t header[8];
    header[0] = channel;
    header[1] = flags;
    header[2] = 0;
    header[3] = 0;
    header[4] = (uint8_t)(len & 0xFF);
    header[5] = (uint8_t)((len >> 8) & 0xFF);
    header[6] = (uint8_t)((len >> 16) & 0xFF);
    header[7] = (uint8_t)((len >> 24) & 0xFF);

    if (qemu_chr_fe_write_all(&s->chr, header, sizeof(header)) != sizeof(header)) {
        qemu_log_mask(LOG_GUEST_ERROR, "ssd1677: ipc header short write\n");
        return;
    }
    if (len > 0 && qemu_chr_fe_write_all(&s->chr, payload, len) != (int)len) {
        qemu_log_mask(LOG_GUEST_ERROR, "ssd1677: ipc payload short write\n");
    }
}

static void push_framebuffer(SSD1677State *s, bool partial)
{
    ipc_send_frame(s, IPC_CHANNEL_FRAMEBUFFER,
                   partial ? IPC_FLAG_FB_PARTIAL : IPC_FLAG_FB_FULL,
                   s->fb, EPD_FB_BYTES);
}

/* ------------------------------ BUSY ------------------------------------- */

static void busy_set(SSD1677State *s, bool high)
{
    s->busy_high = high;
    qemu_set_irq(s->busy_out, high ? 1 : 0);
}

static void busy_timer_expired(void *opaque)
{
    SSD1677State *s = opaque;
    busy_set(s, false);
}

static int refresh_delay_ms(SSD1677State *s)
{
    /* Crude classification from the command 0x22 byte. Patterns: full=0xF7,
     * fast=0xC7, partial=0xFF/0xCF — vendor demos vary; tune in PR2 once
     * we capture real firmware bytes. */
    uint8_t v = s->display_update_ctl_2;
    if (v == 0xF7 || v == 0xC0) {
        return REFRESH_MS_FULL;
    } else if (v == 0xC7) {
        return REFRESH_MS_FAST;
    } else {
        return REFRESH_MS_PARTIAL;
    }
}

/* ---------------------- RAM-write helpers -------------------------------- */

static void advance_ram_pointer(SSD1677State *s)
{
    /* data_entry_mode bit 0: AM (Address increment direction)
     *                       0 = X direction first, 1 = Y direction first
     * data_entry_mode bit 1: ID Y direction (0 = decrement, 1 = increment)
     * data_entry_mode bit 2: ID X direction (0 = decrement, 1 = increment)
     * For our simulator we model the most common case: AM=0, both increment.
     * Full conformance can be added later once captured-traffic shows the
     * firmware uses other modes. */

    uint16_t x_step_dir = (s->data_entry_mode & 0x04) ? +1 : -1;
    uint16_t y_step_dir = (s->data_entry_mode & 0x02) ? +1 : -1;
    bool y_first = (s->data_entry_mode & 0x01) != 0;

    if (y_first) {
        s->ram_y_cur = (uint16_t)(s->ram_y_cur + y_step_dir);
        if (s->ram_y_cur > s->ram_y_end) {
            s->ram_y_cur = s->ram_y_start;
            s->ram_x_cur = (uint16_t)(s->ram_x_cur + x_step_dir);
        }
    } else {
        s->ram_x_cur = (uint16_t)(s->ram_x_cur + x_step_dir);
        if (s->ram_x_cur > s->ram_x_end) {
            s->ram_x_cur = s->ram_x_start;
            s->ram_y_cur = (uint16_t)(s->ram_y_cur + y_step_dir);
        }
    }
}

static void ram_write_byte(SSD1677State *s, uint8_t *target, uint8_t value)
{
    /* ram_x_cur is in 8-pixel units, ram_y_cur is in pixel rows. */
    uint32_t row = s->ram_y_cur;
    uint32_t col_byte = s->ram_x_cur;
    if (row < EPD_HEIGHT && col_byte < EPD_ROW_STRIDE_BYTES) {
        target[row * EPD_ROW_STRIDE_BYTES + col_byte] = value;
    }
    advance_ram_pointer(s);
}

/* ----------------------- Command dispatch -------------------------------- */

static void reset_state(SSD1677State *s)
{
    s->mode = PARSE_IDLE;
    s->current_cmd = 0;
    s->arg_index = 0;
    s->arg_expected = 0;
    s->lut_consume_remaining = 0;
    s->deep_sleep = false;
    s->ram_x_start = 0;
    s->ram_x_end = EPD_ROW_STRIDE_BYTES - 1;
    s->ram_y_start = 0;
    s->ram_y_end = EPD_HEIGHT - 1;
    s->ram_x_cur = 0;
    s->ram_y_cur = 0;
    s->data_entry_mode = 0x03;  /* AM=0, IDX=1, IDY=1 */
    s->display_update_ctl_2 = 0;
    busy_set(s, false);
}

static void execute_command_no_args(SSD1677State *s, uint8_t cmd)
{
    switch (cmd) {
    case CMD_SW_RESET:
        memset(s->fb, 0xFF, EPD_FB_BYTES);
        memset(s->fb_red, 0xFF, EPD_FB_BYTES);
        reset_state(s);
        /* Spec: brief BUSY high after reset */
        busy_set(s, true);
        timer_mod(s->busy_timer,
                  qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 10);
        break;
    case CMD_DEEP_SLEEP:
        /* Real hardware needs HW reset to wake; we accept any next command
         * because production firmware sometimes pokes regs after deep sleep
         * during shutdown sequences. */
        s->deep_sleep = true;
        break;
    case CMD_MASTER_ACTIVATION:
        /* Phase ordering matters: the firmware issues 0x22 (with phase byte)
         * BEFORE 0x20. We've stored the byte; commit framebuffer now and hold
         * BUSY for the configured refresh duration. */
        push_framebuffer(s, false);
        busy_set(s, true);
        timer_mod(s->busy_timer,
                  qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + refresh_delay_ms(s));
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ssd1677: unhandled no-arg cmd 0x%02X\n", cmd);
        break;
    }
}

static int command_arg_count(uint8_t cmd)
{
    switch (cmd) {
    case CMD_DRIVER_OUTPUT_CTL:    return 3;
    case CMD_GATE_VOLTAGE:         return 1;
    case CMD_SOURCE_VOLTAGE:       return 3;
    case CMD_SOFTSTART:            return 4;
    case CMD_DATA_ENTRY_MODE:      return 1;
    case CMD_TEMPERATURE_SENSOR:   return 1;
    case CMD_DISPLAY_UPDATE_CTL_1: return 2;
    case CMD_DISPLAY_UPDATE_CTL_2: return 1;
    case CMD_VCOM_CTL:             return 1;
    case CMD_BORDER_WAVEFORM:      return 1;
    case CMD_RAM_X_START_END:      return 2;
    case CMD_RAM_Y_START_END:      return 4;
    case CMD_AUTO_WRITE_RED:       return 1;
    case CMD_AUTO_WRITE_BW:        return 1;
    case CMD_RAM_X_COUNTER:        return 1;
    case CMD_RAM_Y_COUNTER:        return 2;
    default:                       return 0;
    }
}

static void apply_collected_args(SSD1677State *s)
{
    switch (s->current_cmd) {
    case CMD_DATA_ENTRY_MODE:
        s->data_entry_mode = s->arg_buf[0];
        break;
    case CMD_DISPLAY_UPDATE_CTL_2:
        s->display_update_ctl_2 = s->arg_buf[0];
        break;
    case CMD_RAM_X_START_END:
        s->ram_x_start = s->arg_buf[0];
        s->ram_x_end   = s->arg_buf[1];
        break;
    case CMD_RAM_Y_START_END:
        s->ram_y_start = (uint16_t)s->arg_buf[0] | ((uint16_t)s->arg_buf[1] << 8);
        s->ram_y_end   = (uint16_t)s->arg_buf[2] | ((uint16_t)s->arg_buf[3] << 8);
        break;
    case CMD_RAM_X_COUNTER:
        s->ram_x_cur = s->arg_buf[0];
        break;
    case CMD_RAM_Y_COUNTER:
        s->ram_y_cur = (uint16_t)s->arg_buf[0] | ((uint16_t)s->arg_buf[1] << 8);
        break;
    case CMD_AUTO_WRITE_BW: {
        uint8_t pat = s->arg_buf[0];
        memset(s->fb, pat, EPD_FB_BYTES);
        break;
    }
    case CMD_AUTO_WRITE_RED: {
        uint8_t pat = s->arg_buf[0];
        memset(s->fb_red, pat, EPD_FB_BYTES);
        break;
    }
    case CMD_DRIVER_OUTPUT_CTL:
    case CMD_SOFTSTART:
    case CMD_TEMPERATURE_SENSOR:
    case CMD_BORDER_WAVEFORM:
    case CMD_DISPLAY_UPDATE_CTL_1:
    case CMD_VCOM_CTL:
    case CMD_GATE_VOLTAGE:
    case CMD_SOURCE_VOLTAGE:
        /* Skip-and-consume per Hybrid FSM design. */
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ssd1677: unhandled cmd 0x%02X with %d args\n",
                      s->current_cmd, s->arg_index);
        break;
    }
}

/* ----------------------- SSI transfer hook ------------------------------- */

static uint32_t ssd1677_transfer(SSIPeripheral *dev, uint32_t value)
{
    SSD1677State *s = SSD1677_GDEQ0426(dev);
    uint8_t byte = value & 0xFF;

    if (s->reset_low) {
        return 0;
    }

    if (!s->dc_high) {
        /* Command byte. Aborts any in-progress RAM write. */
        s->current_cmd = byte;
        s->arg_index = 0;
        s->arg_expected = command_arg_count(byte);

        if (byte == CMD_WRITE_RAM_BW || byte == CMD_WRITE_RAM_RED) {
            s->mode = PARSE_RAM_WRITE;
        } else if (byte == CMD_WRITE_LUT) {
            /* SSD1677 LUT is up to 153 bytes. We accept a generous budget
             * and consume until next command; real firmwares are bounded. */
            s->mode = PARSE_LUT_CONSUME;
            s->lut_consume_remaining = 200;
        } else if (s->arg_expected > 0) {
            s->mode = PARSE_COLLECTING_ARGS;
        } else {
            s->mode = PARSE_IDLE;
            execute_command_no_args(s, byte);
        }
        return 0;
    }

    /* Data byte. */
    switch (s->mode) {
    case PARSE_COLLECTING_ARGS:
        if (s->arg_index < sizeof(s->arg_buf)) {
            s->arg_buf[s->arg_index++] = byte;
        }
        if (s->arg_index >= s->arg_expected) {
            apply_collected_args(s);
            s->mode = PARSE_IDLE;
        }
        break;
    case PARSE_RAM_WRITE: {
        uint8_t *target = (s->current_cmd == CMD_WRITE_RAM_BW) ? s->fb : s->fb_red;
        ram_write_byte(s, target, byte);
        break;
    }
    case PARSE_LUT_CONSUME:
        if (s->lut_consume_remaining > 0) {
            s->lut_consume_remaining--;
        }
        break;
    case PARSE_IDLE:
    default:
        /* Stray data byte before any command — log and ignore. */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ssd1677: unexpected data 0x%02X in IDLE\n", byte);
        break;
    }
    return 0;
}

/* ----------------------- GPIO sinks (D/C#, RES#) ------------------------- */

static void dc_irq_handler(void *opaque, int n, int level)
{
    SSD1677State *s = opaque;
    s->dc_high = (level != 0);
}

static void reset_irq_handler(void *opaque, int n, int level)
{
    SSD1677State *s = opaque;
    bool was_low = s->reset_low;
    s->reset_low = (level == 0);
    if (was_low && !s->reset_low) {
        /* Rising edge — perform HW reset. */
        memset(s->fb, 0xFF, EPD_FB_BYTES);
        memset(s->fb_red, 0xFF, EPD_FB_BYTES);
        reset_state(s);
    }
}

/* ----------------------- QEMU plumbing ----------------------------------- */

static const VMStateDescription vmstate_ssd1677 = {
    .name = TYPE_SSD1677_GDEQ0426,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_SSI_PERIPHERAL(parent_obj, SSD1677State),
        VMSTATE_BOOL(dc_high, SSD1677State),
        VMSTATE_BOOL(reset_low, SSD1677State),
        VMSTATE_BOOL(busy_high, SSD1677State),
        VMSTATE_UINT8(mode, SSD1677State),
        VMSTATE_UINT8(current_cmd, SSD1677State),
        VMSTATE_UINT8_ARRAY(arg_buf, SSD1677State, 8),
        VMSTATE_UINT8(arg_index, SSD1677State),
        VMSTATE_UINT8(arg_expected, SSD1677State),
        VMSTATE_UINT16(lut_consume_remaining, SSD1677State),
        VMSTATE_BOOL(deep_sleep, SSD1677State),
        VMSTATE_UINT16(ram_x_start, SSD1677State),
        VMSTATE_UINT16(ram_x_end, SSD1677State),
        VMSTATE_UINT16(ram_y_start, SSD1677State),
        VMSTATE_UINT16(ram_y_end, SSD1677State),
        VMSTATE_UINT16(ram_x_cur, SSD1677State),
        VMSTATE_UINT16(ram_y_cur, SSD1677State),
        VMSTATE_UINT8(data_entry_mode, SSD1677State),
        VMSTATE_UINT8(display_update_ctl_2, SSD1677State),
        VMSTATE_UINT8_ARRAY(fb, SSD1677State, EPD_FB_BYTES),
        VMSTATE_UINT8_ARRAY(fb_red, SSD1677State, EPD_FB_BYTES),
        VMSTATE_END_OF_LIST()
    }
};

static const Property ssd1677_properties[] = {
    DEFINE_PROP_CHR("chardev", SSD1677State, chr),
};

static void ssd1677_realize(SSIPeripheral *d, Error **errp)
{
    SSD1677State *s = SSD1677_GDEQ0426(d);
    DeviceState *dev = DEVICE(d);

    qdev_init_gpio_in_named(dev, dc_irq_handler, "dc", 1);
    qdev_init_gpio_in_named(dev, reset_irq_handler, "reset", 1);
    qdev_init_gpio_out_named(dev, &s->busy_out, "busy", 1);

    s->busy_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL,
                                 busy_timer_expired, s);

    memset(s->fb, 0xFF, EPD_FB_BYTES);
    memset(s->fb_red, 0xFF, EPD_FB_BYTES);
    reset_state(s);
}

static void ssd1677_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);

    k->realize  = ssd1677_realize;
    k->transfer = ssd1677_transfer;
    dc->vmsd    = &vmstate_ssd1677;
    device_class_set_props(dc, ssd1677_properties);
    dc->desc    = "GoodDisplay GDEQ0426T82-T01C 4.26\" e-ink, SSD1677 driver";
}

static const TypeInfo ssd1677_info = {
    .name           = TYPE_SSD1677_GDEQ0426,
    .parent         = TYPE_SSI_PERIPHERAL,
    .instance_size  = sizeof(SSD1677State),
    .class_init     = ssd1677_class_init,
};

static void ssd1677_register_types(void)
{
    type_register_static(&ssd1677_info);
}

type_init(ssd1677_register_types)
