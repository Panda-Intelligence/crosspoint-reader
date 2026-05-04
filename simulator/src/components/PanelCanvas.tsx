import { useEffect, useRef } from "react";
import { listen, UnlistenFn } from "@tauri-apps/api/event";

const EPD_WIDTH = 800;
const EPD_HEIGHT = 480;

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
 * RGBA8888 pixel data sent by the QEMU SSD1677 virtual peripheral. The
 * Tauri-side IPC layer (src-tauri/src/ipc.rs) decodes the 1bpp wire format
 * to RGBA before emitting; this component just blits.
 */
export function PanelCanvas() {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const lastUpdateRef = useRef<{ kind: string; ts: number } | null>(null);

  useEffect(() => {
    let unlisten: UnlistenFn | null = null;
    (async () => {
      unlisten = await listen<FramebufferEvent>("framebuffer", (e) => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        if (!ctx) return;
        const { width, height, rgba_b64, kind } = e.payload;
        if (width !== EPD_WIDTH || height !== EPD_HEIGHT) {
          // Defensive: panel size is fixed for Mofei. Guard against future
          // PR2 backend changes that might emit different dimensions.
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
        lastUpdateRef.current = { kind, ts: performance.now() };
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
    // Subtle bezel hint
    ctx.strokeStyle = "#888";
    ctx.lineWidth = 1;
    ctx.strokeRect(0.5, 0.5, EPD_WIDTH - 1, EPD_HEIGHT - 1);
  }, []);

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
          // Render at 1:1 by default; the surrounding layout decides on
          // CSS scaling. Image-rendering pixelated keeps 1bpp art crisp
          // when the user resizes the window.
          imageRendering: "pixelated",
          background: "#fff",
        }}
      />
    </div>
  );
}
