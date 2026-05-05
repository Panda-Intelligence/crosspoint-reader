import { useEffect, useRef, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, UnlistenFn } from "@tauri-apps/api/event";

const EPD_WIDTH = 800;
const EPD_HEIGHT = 480;

const TOUCH_DOWN = 1;
const TOUCH_MOVE = 2;
const TOUCH_UP = 3;

type FramebufferEvent = {
  kind: "full" | "partial";
  width: number;
  height: number;
  rgba_b64: string;
};

function decodeBase64ToUint8(b64: string): Uint8Array {
  const bin = atob(b64);
  const arr = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
  return arr;
}

/**
 * E-ink panel canvas. Subscribes to the "framebuffer" Tauri event and blits
 * RGBA8888 pixel data sent by the QEMU SSD1677 virtual peripheral. Mouse
 * events on the canvas surface are converted to FT6336U panel coordinates
 * and sent via the inject_touch Tauri command.
 *
 * The Tauri-side IPC layer (src-tauri/src/ipc.rs) decodes the 1bpp wire
 * format to RGBA before emitting the framebuffer event; this component
 * just blits.
 */
export function PanelCanvas() {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const trackingRef = useRef<{ active: boolean }>({ active: false });

  // Subscribe to framebuffer events.
  useEffect(() => {
    let unlisten: UnlistenFn | null = null;
    (async () => {
      unlisten = await listen<FramebufferEvent>("framebuffer", (e) => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        if (!ctx) return;
        const { width, height, rgba_b64 } = e.payload;
        if (width !== EPD_WIDTH || height !== EPD_HEIGHT) {
          // eslint-disable-next-line no-console
          console.warn(
            `framebuffer event with unexpected size ${width}x${height}, expected ${EPD_WIDTH}x${EPD_HEIGHT}`,
          );
          return;
        }
        const bytes = decodeBase64ToUint8(rgba_b64);
        if (bytes.length !== width * height * 4) {
          // eslint-disable-next-line no-console
          console.warn(
            `framebuffer event payload ${bytes.length} bytes, expected ${width * height * 4}`,
          );
          return;
        }
        const image = new ImageData(
          new Uint8ClampedArray(bytes.buffer, bytes.byteOffset, bytes.byteLength),
          width,
          height,
        );
        ctx.putImageData(image, 0, 0);
      });
    })();
    return () => {
      if (unlisten) unlisten();
    };
  }, []);

  // Initialize canvas with all-white (matches SSD1677 power-on state).
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, EPD_WIDTH, EPD_HEIGHT);
    ctx.strokeStyle = "#888";
    ctx.lineWidth = 1;
    ctx.strokeRect(0.5, 0.5, EPD_WIDTH - 1, EPD_HEIGHT - 1);
  }, []);

  // Convert pointer event to panel-native coordinates accounting for any
  // CSS scaling. The canvas's intrinsic size is fixed at 800×480 but the
  // displayed size may be smaller on a constrained window.
  const eventToPanelXY = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>): { x: number; y: number } | null => {
      const canvas = canvasRef.current;
      if (!canvas) return null;
      const rect = canvas.getBoundingClientRect();
      if (rect.width === 0 || rect.height === 0) return null;
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;
      const px = Math.max(0, Math.min(canvas.width - 1,
        Math.round((e.clientX - rect.left) * scaleX)));
      const py = Math.max(0, Math.min(canvas.height - 1,
        Math.round((e.clientY - rect.top) * scaleY)));
      return { x: px, y: py };
    },
    [],
  );

  const sendTouch = useCallback(async (action: number, x: number, y: number) => {
    try {
      await invoke<boolean>("inject_touch", {
        action,
        x,
        y,
        fingerId: 0,
      });
    } catch (e) {
      // Inject failures during simulator-not-running are expected; ignore.
      // eslint-disable-next-line no-console
      console.debug("inject_touch failed:", e);
    }
  }, []);

  const onMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const xy = eventToPanelXY(e);
    if (!xy) return;
    trackingRef.current.active = true;
    void sendTouch(TOUCH_DOWN, xy.x, xy.y);
  };
  const onMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!trackingRef.current.active) return;
    const xy = eventToPanelXY(e);
    if (!xy) return;
    void sendTouch(TOUCH_MOVE, xy.x, xy.y);
  };
  const onMouseUp = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!trackingRef.current.active) return;
    trackingRef.current.active = false;
    const xy = eventToPanelXY(e);
    if (xy) void sendTouch(TOUCH_UP, xy.x, xy.y);
  };
  const onMouseLeave = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!trackingRef.current.active) return;
    trackingRef.current.active = false;
    const xy = eventToPanelXY(e);
    if (xy) void sendTouch(TOUCH_UP, xy.x, xy.y);
  };

  return (
    <div
      style={{
        background: "#222",
        padding: 12,
        borderRadius: 6,
        display: "inline-block",
      }}
      aria-label="E-ink panel"
    >
      <canvas
        ref={canvasRef}
        width={EPD_WIDTH}
        height={EPD_HEIGHT}
        style={{
          display: "block",
          imageRendering: "pixelated",
          background: "#fff",
          cursor: "crosshair",
        }}
        onMouseDown={onMouseDown}
        onMouseMove={onMouseMove}
        onMouseUp={onMouseUp}
        onMouseLeave={onMouseLeave}
      />
    </div>
  );
}
