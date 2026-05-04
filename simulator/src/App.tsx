import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, UnlistenFn } from "@tauri-apps/api/event";
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

  return (
    <main className="container">
      <h1>Mofei Simulator</h1>
      <p style={{ marginTop: 0, color: "#888" }}>
        ESP32-S3 + GDEQ0426T82 + FT6336U — PR1 (M1: serial console only)
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

      <p style={{ fontSize: 12, color: "#666", margin: "4px 0" }}>{status}</p>

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

      <div
        style={{
          background: "#111",
          color: "#0f0",
          fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
          fontSize: 12,
          padding: 8,
          borderRadius: 4,
          height: 360,
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
