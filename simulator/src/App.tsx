import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, UnlistenFn } from "@tauri-apps/api/event";
import { PanelCanvas } from "./components/PanelCanvas";
import "./App.css";

const MAX_LOG_LINES = 1000;

function App() {
  const [running, setRunning] = useState(false);
  const [status, setStatus] = useState("Idle");
  const [error, setError] = useState<string | null>(null);
  const [logLines, setLogLines] = useState<string[]>([]);
  const logEndRef = useRef<HTMLDivElement | null>(null);

  // Subscribe to backend events for the lifetime of the component.
  useEffect(() => {
    let serialUnlisten: UnlistenFn | null = null;
    let errorUnlisten: UnlistenFn | null = null;

    (async () => {
      serialUnlisten = await listen<string>("serial-log", (e) => {
        setLogLines((prev) => {
          // Split incoming chunk on newlines, append, cap at MAX_LOG_LINES.
          const incoming = e.payload.split("\n");
          const next = [...prev, ...incoming];
          if (next.length > MAX_LOG_LINES) {
            return next.slice(next.length - MAX_LOG_LINES);
          }
          return next;
        });
      });
      errorUnlisten = await listen<string>("simulator-error", (e) => {
        setError(e.payload);
      });
    })();

    return () => {
      if (serialUnlisten) serialUnlisten();
      if (errorUnlisten) errorUnlisten();
    };
  }, []);

  // Auto-scroll the console pane to the bottom on new lines.
  useEffect(() => {
    if (logEndRef.current) {
      logEndRef.current.scrollIntoView({ block: "end" });
    }
  }, [logLines]);

  const startSim = async () => {
    setError(null);
    try {
      const res = await invoke<string>("start_sim");
      setStatus(`start_sim: ${res}`);
      if (res === "started" || res === "already_running") {
        setRunning(true);
      }
    } catch (e) {
      const msg = typeof e === "string" ? e : (e as Error).message ?? String(e);
      setError(msg);
      setStatus("start_sim: error");
    }
  };

  const stopSim = async () => {
    setError(null);
    try {
      const res = await invoke<string>("stop_sim");
      setStatus(`stop_sim: ${res}`);
      setRunning(false);
    } catch (e) {
      const msg = typeof e === "string" ? e : (e as Error).message ?? String(e);
      setError(msg);
      setStatus("stop_sim: error");
    }
  };

  const clearLog = () => setLogLines([]);

  // ---- Keyboard input injection ----
  // Maps:
  //   ArrowLeft / ArrowRight / ArrowUp / ArrowDown → simulated swipe
  //     (touch DOWN at panel center, MOVE 200px in direction over ~150ms,
  //     touch UP)
  //   Space / Enter → tap at panel center (DOWN+UP)
  //   PageUp / PageDown → side-button events (button 0 / 1)
  //   Escape → Home button (button 2)
  //   "r" / "R" → Hardware Refresh button (button 3)
  // See ipc.rs::encode_button_event for the wire layout.
  useEffect(() => {
    const PANEL_W = 800;
    const PANEL_H = 480;
    const cx = PANEL_W / 2;
    const cy = PANEL_H / 2;
    const SWIPE_PX = 200;

    const sendTouch = (action: number, x: number, y: number) =>
      invoke<boolean>("inject_touch", { action, x, y, fingerId: 0 }).catch(() => false);
    const sendButton = (button: number, pressed: boolean) =>
      invoke<boolean>("inject_button", { buttonId: button, pressed }).catch(() => false);

    const sendSwipe = async (dx: number, dy: number) => {
      const ex = Math.max(0, Math.min(PANEL_W - 1, cx + dx));
      const ey = Math.max(0, Math.min(PANEL_H - 1, cy + dy));
      await sendTouch(1, cx, cy); // DOWN
      // Three intermediate MOVE points feels more natural than one.
      for (let i = 1; i <= 3; i++) {
        const t = i / 3;
        await new Promise((r) => setTimeout(r, 50));
        await sendTouch(2, Math.round(cx + dx * t), Math.round(cy + dy * t));
      }
      await sendTouch(3, ex, ey); // UP
    };

    const sendTap = async () => {
      await sendTouch(1, cx, cy);
      await new Promise((r) => setTimeout(r, 50));
      await sendTouch(3, cx, cy);
    };

    const onKeyDown = (e: KeyboardEvent) => {
      // Skip if a typeable element has focus — we don't want to steal input
      // from text fields the user might add later.
      const tag = (e.target as HTMLElement | null)?.tagName?.toLowerCase();
      if (tag === "input" || tag === "textarea") return;

      switch (e.key) {
        case "ArrowLeft":
          e.preventDefault();
          void sendSwipe(-SWIPE_PX, 0);
          break;
        case "ArrowRight":
          e.preventDefault();
          void sendSwipe(SWIPE_PX, 0);
          break;
        case "ArrowUp":
          e.preventDefault();
          void sendSwipe(0, -SWIPE_PX);
          break;
        case "ArrowDown":
          e.preventDefault();
          void sendSwipe(0, SWIPE_PX);
          break;
        case " ":
        case "Enter":
          e.preventDefault();
          void sendTap();
          break;
        case "PageUp":
          e.preventDefault();
          void sendButton(0, true);
          break;
        case "PageDown":
          e.preventDefault();
          void sendButton(1, true);
          break;
        case "Escape":
          e.preventDefault();
          void sendButton(2, true);
          break;
        case "r":
        case "R":
          if (!e.metaKey && !e.ctrlKey) {
            e.preventDefault();
            void sendButton(3, true);
          }
          break;
        default:
          break;
      }
    };

    const onKeyUp = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement | null)?.tagName?.toLowerCase();
      if (tag === "input" || tag === "textarea") return;

      switch (e.key) {
        case "PageUp":
          void sendButton(0, false);
          break;
        case "PageDown":
          void sendButton(1, false);
          break;
        case "Escape":
          void sendButton(2, false);
          break;
        case "r":
        case "R":
          if (!e.metaKey && !e.ctrlKey) {
            void sendButton(3, false);
          }
          break;
        default:
          break;
      }
    };

    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
    };
  }, []);

  return (
    <main className="container">
      <h1>Mofei Simulator</h1>
      <p style={{ marginTop: 0, color: "#888" }}>
        ESP32-S3 + GDEQ0426T82 + FT6336U — PR3 (M3: input injection)
      </p>

      <div className="card" style={{ display: "flex", gap: 8, alignItems: "center" }}>
        {!running ? (
          <button onClick={startSim} style={{ background: "#4CAF50", color: "#fff" }}>
            Launch QEMU
          </button>
        ) : (
          <button onClick={stopSim} style={{ background: "#F44336", color: "#fff" }}>
            Stop
          </button>
        )}
        <button onClick={clearLog} style={{ marginLeft: "auto" }}>
          Clear log
        </button>
      </div>

      <p style={{ fontSize: 12, color: "#666", margin: "4px 0" }}>
        {status}
        {" · "}
        <span style={{ color: "#888" }}>
          Keys: ←→↑↓ swipe · Space/Enter tap · PgUp/PgDn side buttons · Esc Home · R refresh
        </span>
      </p>

      {error && (
        <pre
          style={{
            background: "#fee",
            color: "#a00",
            padding: 8,
            borderRadius: 4,
            whiteSpace: "pre-wrap",
            fontSize: 12,
          }}
        >
          {error}
        </pre>
      )}

      <div style={{ display: "flex", justifyContent: "center", margin: "12px 0" }}>
        <PanelCanvas />
      </div>

      <div
        style={{
          background: "#111",
          color: "#0f0",
          fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
          fontSize: 12,
          padding: 8,
          borderRadius: 4,
          height: 240,
          overflowY: "auto",
          whiteSpace: "pre-wrap",
          wordBreak: "break-all",
        }}
        aria-label="Serial console"
      >
        {logLines.length === 0 ? (
          <span style={{ color: "#444" }}>(no serial output yet)</span>
        ) : (
          logLines.map((line, i) => <div key={i}>{line || "\u00a0"}</div>)
        )}
        <div ref={logEndRef} />
      </div>
    </main>
  );
}

export default App;
