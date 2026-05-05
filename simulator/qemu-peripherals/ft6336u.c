/*
 * FT6336U virtual capacitive touch controller for the Mofei simulator.
 *
 * Implements the Minimal Register Machine design from
 *   .trellis/tasks/05-04-mofei-simulator-bringup/research/ft6336u-i2c-modeling.md
 * Option A: only registers MofeiTouchDriver actually reads/writes
 * (0x00, 0x01, 0x02..0x0E, 0x80, 0x88, 0xA6, 0xA8) are modeled with state;
 * everything else returns 0 on read and is no-op on write.
 *
 * Wire to host UI: chardev with the 8-byte length-prefixed binary frame
 * format (see qemu-peripheral-ipc.md). Reads inbound channel 0x04 (touch
 * events) from the host, populates a 11-byte FT6336U frame buffer at
 * register 0x02, asserts INT (active-low GPIO out) until the firmware
 * issues the 11-byte burst read.
 *
 * The Mofei firmware uses BIT-BANG software I2C (MOFEI_TOUCH_SOFT_I2C=1):
 * SCL=GPIO12, SDA=GPIO13. Therefore this peripheral does NOT attach to the
 * ESP32-S3 I2C controller. It attaches as a virtual I2C slave on a
 * BITBANG_I2C bus which is itself wired to the GPIO matrix lines for
 * GPIO12/GPIO13. See `hw/xtensa/esp32s3.c` integration notes in the
 * companion patch.
 *
 * Integration status (PR3): C source committed, **not yet wired into the
 * QEMU build**. See simulator/qemu-peripherals/README.md.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"
#include "chardev/char-fe.h"
#include "migration/vmstate.h"

#define TYPE_FT6336U "ft6336u-mofei"
OBJECT_DECLARE_SIMPLE_TYPE(FT6336UState, FT6336U)

/* Locked from .trellis/tasks/04-29-mofei-touch-support/prd.md and lib/hal/MofeiTouch.h */
#define FT6336U_DEFAULT_ADDR  0x2E

/* IPC channel tags — keep in sync with simulator/src-tauri/src/ipc.rs */
#define IPC_CHANNEL_TOUCH_EVENT  0x04

/* Touch action codes (matches host-side TouchAction enum in ipc.rs) */
#define TOUCH_DOWN  0x01
#define TOUCH_MOVE  0x02
#define TOUCH_UP    0x03

/* FT6336U register addresses (subset that MofeiTouchDriver actually touches) */
#define REG_DEVICE_MODE       0x00
#define REG_GESTURE_ID        0x01
#define REG_TD_STATUS         0x02
#define REG_P1_XH             0x03
#define REG_P1_XL             0x04
#define REG_P1_YH             0x05
#define REG_P1_YL             0x06
#define REG_P1_WEIGHT         0x07
#define REG_P1_MISC           0x08
#define REG_P2_XH             0x09
#define REG_P2_XL             0x0A
#define REG_P2_YH             0x0B
#define REG_P2_YL             0x0C
#define REG_THGROUP           0x80
#define REG_PERIODACTIVE      0x88
#define REG_FIRMWARE_ID       0xA6
#define REG_VENDOR_ID         0xA8

/* FT6336U event codes in P1_XH/P2_XH bits 7:6 */
#define EVT_PUT_DOWN  0x00
#define EVT_PUT_UP    0x01
#define EVT_CONTACT   0x02

#define MAX_POINTS    2
#define MAX_RAW_X     800
#define MAX_RAW_Y     480

#define HEADER_LEN 8
#define MAX_INBOUND_PAYLOAD 64

struct FT6336UState {
    I2CSlave parent_obj;

    CharBackend chr;             /* host IPC */
    qemu_irq    int_pin;         /* INT GPIO line (active-low) */

    /* I2C state machine */
    uint8_t reg_ptr;             /* last register address received */
    bool    expecting_reg_addr;  /* true if next master-write byte is the register pointer */

    /* Register values for those that have meaningful state */
    uint8_t device_mode;         /* 0x00 */
    uint8_t gesture_id;          /* 0x01 */
    uint8_t thgroup;             /* 0x80 */
    uint8_t periodactive;        /* 0x88 */
    uint8_t firmware_id;         /* 0xA6 — return canned 0x01 */
    uint8_t vendor_id;           /* 0xA8 — return FocalTech 0x11 */

    /* Touch frame buffer (11 bytes starting at reg 0x02) */
    uint8_t frame[11];           /* TD_STATUS (idx 0) + 2 × 5-byte point (idx 1..10) */

    /* Pending event from host — applied to frame on next master-read of TD_STATUS */
    uint16_t cur_x[MAX_POINTS];
    uint16_t cur_y[MAX_POINTS];
    uint8_t  cur_evt[MAX_POINTS];
    uint8_t  cur_count;
    bool     have_pending;

    /* Inbound-frame parser state */
    uint8_t  inbound_header[HEADER_LEN];
    uint8_t  inbound_header_filled;
    uint8_t  inbound_payload[MAX_INBOUND_PAYLOAD];
    uint32_t inbound_payload_target;
    uint32_t inbound_payload_filled;
};

/* ---------------------- Frame buffer helpers ----------------------------- */

static void encode_frame(FT6336UState *s)
{
    memset(s->frame, 0, sizeof(s->frame));
    s->frame[0] = s->cur_count & 0x0F;  /* high nibble must be zero */
    for (int i = 0; i < MAX_POINTS && i < s->cur_count; i++) {
        uint16_t x = s->cur_x[i];
        uint16_t y = s->cur_y[i];
        if (x >= MAX_RAW_X) x = MAX_RAW_X - 1;
        if (y >= MAX_RAW_Y) y = MAX_RAW_Y - 1;

        uint8_t  evt = s->cur_evt[i] & 0x03;
        uint8_t  off = 1 + i * 5;  /* 1, 6 — but firmware reads 11 bytes total, so i==1 reaches up to byte 10 */
        s->frame[off + 0] = (uint8_t)((evt << 6) | ((x >> 8) & 0x0F));
        s->frame[off + 1] = (uint8_t)(x & 0xFF);
        s->frame[off + 2] = (uint8_t)(((i & 0x0F) << 4) | ((y >> 8) & 0x0F));
        s->frame[off + 3] = (uint8_t)(y & 0xFF);
        s->frame[off + 4] = 0x40;   /* WEIGHT canned */
    }
}

static void int_set(FT6336UState *s, bool active_low)
{
    /* MofeiTouchDriver treats INT as active-low level: digitalRead(INT)==LOW
     * → data ready. Drive 0 when active, 1 when idle. */
    qemu_set_irq(s->int_pin, active_low ? 0 : 1);
}

static void apply_pending(FT6336UState *s)
{
    if (!s->have_pending) {
        return;
    }
    encode_frame(s);
    s->have_pending = false;
    int_set(s, true);  /* assert INT */
}

/* ----------------------- IPC inbound parsing ----------------------------- */

static void handle_touch_event(FT6336UState *s, const uint8_t *payload, uint32_t len)
{
    /* Wire format (cf. ipc.rs TouchEvent::serialize):
     *   u8  action (0x01 DOWN, 0x02 MOVE, 0x03 UP)
     *   u16 x      (little-endian, panel raw, 0..799)
     *   u16 y      (little-endian, panel raw, 0..479)
     *   u8  finger_id (0 or 1)
     *
     * 6 bytes per event. Multiple events may be batched in one frame
     * payload. */
    const uint32_t per_event = 6;
    if (len == 0 || (len % per_event) != 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ft6336u: malformed touch payload len=%u\n", len);
        return;
    }

    /* For multi-finger we accumulate per-finger latest state. Reset the
     * per-finger event presence bitmap; finger 0 / 1 fill what's reported. */
    bool present[MAX_POINTS] = { false, false };
    uint16_t x[MAX_POINTS] = { 0, 0 };
    uint16_t y[MAX_POINTS] = { 0, 0 };
    uint8_t  evt[MAX_POINTS] = { EVT_CONTACT, EVT_CONTACT };

    for (uint32_t off = 0; off < len; off += per_event) {
        uint8_t  action  = payload[off + 0];
        uint16_t ex      = (uint16_t)payload[off + 1] | ((uint16_t)payload[off + 2] << 8);
        uint16_t ey      = (uint16_t)payload[off + 3] | ((uint16_t)payload[off + 4] << 8);
        uint8_t  fid     = payload[off + 5];

        if (fid >= MAX_POINTS) {
            continue;
        }
        present[fid] = (action != TOUCH_UP);
        x[fid] = ex;
        y[fid] = ey;
        switch (action) {
        case TOUCH_DOWN: evt[fid] = EVT_PUT_DOWN; break;
        case TOUCH_MOVE: evt[fid] = EVT_CONTACT; break;
        case TOUCH_UP:   evt[fid] = EVT_PUT_UP;  break;
        default:         evt[fid] = EVT_CONTACT; break;
        }
    }

    s->cur_count = 0;
    for (int i = 0; i < MAX_POINTS; i++) {
        if (present[i] || evt[i] == EVT_PUT_UP) {
            s->cur_x[s->cur_count] = x[i];
            s->cur_y[s->cur_count] = y[i];
            s->cur_evt[s->cur_count] = evt[i];
            s->cur_count++;
        }
    }
    s->have_pending = true;
    apply_pending(s);
}

static void chr_can_read_setup(FT6336UState *s); /* forward */

static int chr_can_read(void *opaque)
{
    return MAX_INBOUND_PAYLOAD;
}

static void chr_read(void *opaque, const uint8_t *buf, int size)
{
    FT6336UState *s = opaque;
    int p = 0;
    while (p < size) {
        if (s->inbound_header_filled < HEADER_LEN) {
            uint32_t need = HEADER_LEN - s->inbound_header_filled;
            uint32_t take = (uint32_t)(size - p);
            if (take > need) take = need;
            memcpy(&s->inbound_header[s->inbound_header_filled], &buf[p], take);
            s->inbound_header_filled += take;
            p += take;
            if (s->inbound_header_filled == HEADER_LEN) {
                s->inbound_payload_target =
                    (uint32_t)s->inbound_header[4]
                    | ((uint32_t)s->inbound_header[5] << 8)
                    | ((uint32_t)s->inbound_header[6] << 16)
                    | ((uint32_t)s->inbound_header[7] << 24);
                if (s->inbound_payload_target > MAX_INBOUND_PAYLOAD) {
                    qemu_log_mask(LOG_GUEST_ERROR,
                                  "ft6336u: inbound payload %u exceeds cap\n",
                                  s->inbound_payload_target);
                    /* Resync: drop header, accept loss. */
                    s->inbound_header_filled = 0;
                    s->inbound_payload_target = 0;
                }
                s->inbound_payload_filled = 0;
            }
        } else {
            uint32_t need = s->inbound_payload_target - s->inbound_payload_filled;
            uint32_t take = (uint32_t)(size - p);
            if (take > need) take = need;
            memcpy(&s->inbound_payload[s->inbound_payload_filled], &buf[p], take);
            s->inbound_payload_filled += take;
            p += take;
            if (s->inbound_payload_filled == s->inbound_payload_target) {
                if (s->inbound_header[0] == IPC_CHANNEL_TOUCH_EVENT) {
                    handle_touch_event(s, s->inbound_payload,
                                       s->inbound_payload_target);
                }
                /* GPIO/button events handled elsewhere — this peripheral
                 * only consumes touch. */
                s->inbound_header_filled = 0;
                s->inbound_payload_target = 0;
                s->inbound_payload_filled = 0;
            }
        }
    }
}

static void chr_event(void *opaque, QEMUChrEvent event)
{
    FT6336UState *s = opaque;
    if (event == CHR_EVENT_OPENED) {
        s->inbound_header_filled = 0;
        s->inbound_payload_target = 0;
        s->inbound_payload_filled = 0;
    }
}

/* ----------------------- I²C slave callbacks ----------------------------- */

static int ft6336u_event(I2CSlave *i2c, enum i2c_event event)
{
    FT6336UState *s = FT6336U(i2c);
    switch (event) {
    case I2C_START_SEND:
        s->expecting_reg_addr = true;
        break;
    case I2C_START_RECV:
        /* If the master is reading TD_STATUS, latch any pending event into
         * the frame buffer now. */
        if (s->reg_ptr == REG_TD_STATUS && s->have_pending) {
            apply_pending(s);
        }
        break;
    case I2C_FINISH:
        /* If the master read 11 bytes starting at TD_STATUS, the touch is
         * delivered — release INT until next event. */
        if (s->reg_ptr == REG_TD_STATUS) {
            int_set(s, false);
        }
        break;
    case I2C_NACK:
        break;
    default:
        break;
    }
    return 0;
}

static int ft6336u_send(I2CSlave *i2c, uint8_t data)
{
    FT6336UState *s = FT6336U(i2c);
    if (s->expecting_reg_addr) {
        s->reg_ptr = data;
        s->expecting_reg_addr = false;
        return 0;
    }
    /* Subsequent bytes are register writes (single-byte register puts). */
    switch (s->reg_ptr) {
    case REG_DEVICE_MODE:    s->device_mode = data; break;
    case REG_THGROUP:        s->thgroup = data; break;
    case REG_PERIODACTIVE:   s->periodactive = data; break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ft6336u: write to reg 0x%02X = 0x%02X (ignored)\n",
                      s->reg_ptr, data);
        break;
    }
    s->reg_ptr++;  /* auto-increment on multi-byte writes */
    return 0;
}

static uint8_t ft6336u_recv(I2CSlave *i2c)
{
    FT6336UState *s = FT6336U(i2c);
    uint8_t value = 0x00;

    switch (s->reg_ptr) {
    case REG_DEVICE_MODE:    value = s->device_mode; break;
    case REG_GESTURE_ID:     value = s->gesture_id; break;
    case REG_TD_STATUS ... REG_P2_YL:
        value = s->frame[s->reg_ptr - REG_TD_STATUS];
        break;
    case REG_THGROUP:        value = s->thgroup; break;
    case REG_PERIODACTIVE:   value = s->periodactive; break;
    case REG_FIRMWARE_ID:    value = s->firmware_id; break;
    case REG_VENDOR_ID:      value = s->vendor_id; break;
    default:                 value = 0x00; break;
    }
    s->reg_ptr++;  /* auto-increment for burst reads */
    return value;
}

/* ----------------------- QEMU plumbing ----------------------------------- */

static void ft6336u_realize(DeviceState *dev, Error **errp)
{
    FT6336UState *s = FT6336U(dev);

    qdev_init_gpio_out_named(dev, &s->int_pin, "int", 1);

    s->reg_ptr = 0;
    s->expecting_reg_addr = true;
    s->device_mode = 0x00;
    s->gesture_id = 0x00;
    s->thgroup = 22;          /* matches FT6336_THGROUP_DEFAULT in firmware */
    s->periodactive = 14;     /* matches FT6336_PERIODACTIVE_DEFAULT */
    s->firmware_id = 0x01;
    s->vendor_id = 0x11;      /* FocalTech */
    s->cur_count = 0;
    s->have_pending = false;
    memset(s->frame, 0, sizeof(s->frame));
    int_set(s, false);  /* idle high */

    if (qemu_chr_fe_backend_connected(&s->chr)) {
        qemu_chr_fe_set_handlers(&s->chr, chr_can_read, chr_read, chr_event,
                                 NULL, s, NULL, true);
    }
    chr_can_read_setup(s);
}

static void chr_can_read_setup(FT6336UState *s)
{
    /* placeholder — kept so chr_can_read can stay 'static int' without
     * forward-decl gymnastics. */
    (void)s;
}

static const VMStateDescription vmstate_ft6336u = {
    .name = TYPE_FT6336U,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_I2C_SLAVE(parent_obj, FT6336UState),
        VMSTATE_UINT8(reg_ptr, FT6336UState),
        VMSTATE_BOOL(expecting_reg_addr, FT6336UState),
        VMSTATE_UINT8(device_mode, FT6336UState),
        VMSTATE_UINT8(gesture_id, FT6336UState),
        VMSTATE_UINT8(thgroup, FT6336UState),
        VMSTATE_UINT8(periodactive, FT6336UState),
        VMSTATE_UINT8(firmware_id, FT6336UState),
        VMSTATE_UINT8(vendor_id, FT6336UState),
        VMSTATE_UINT8_ARRAY(frame, FT6336UState, 11),
        VMSTATE_END_OF_LIST()
    }
};

static const Property ft6336u_properties[] = {
    DEFINE_PROP_CHR("chardev", FT6336UState, chr),
};

static void ft6336u_class_init(ObjectClass *klass, void *data)
{
    DeviceClass    *dc = DEVICE_CLASS(klass);
    I2CSlaveClass  *k  = I2C_SLAVE_CLASS(klass);

    k->event  = ft6336u_event;
    k->send   = ft6336u_send;
    k->recv   = ft6336u_recv;
    dc->realize = ft6336u_realize;
    dc->vmsd  = &vmstate_ft6336u;
    device_class_set_props(dc, ft6336u_properties);
    dc->desc  = "FocalTech FT6336U capacitive touch controller (Mofei)";
}

static const TypeInfo ft6336u_info = {
    .name           = TYPE_FT6336U,
    .parent         = TYPE_I2C_SLAVE,
    .instance_size  = sizeof(FT6336UState),
    .class_init     = ft6336u_class_init,
};

static void ft6336u_register_types(void)
{
    type_register_static(&ft6336u_info);
}

type_init(ft6336u_register_types)
