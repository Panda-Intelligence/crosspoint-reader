import { useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import "./App.css";

function App() {
  const [running, setRunning] = useState(false);
  const [msg, setMsg] = useState("");

  const startSim = async () => {
    try {
      const res = await invoke("start_sim");
      setMsg(`Status: ${res}`);
      setRunning(true);
    } catch (e: any) {
      setMsg(`Error: ${e}`);
    }
  };

  const stopSim = async () => {
    try {
      const res = await invoke("stop_sim");
      setMsg(`Status: ${res}`);
      setRunning(false);
    } catch (e: any) {
      setMsg(`Error: ${e}`);
    }
  };

  return (
    <main className="container">
      <h1>Mofei Simulator</h1>
      <p>Control the native ESP32 Mofei E-ink Simulator</p>

      <div className="card">
        {!running ? (
          <button onClick={startSim} style={{ background: "#4CAF50", color: "#fff" }}>
            Launch Simulator
          </button>
        ) : (
          <button onClick={stopSim} style={{ background: "#F44336", color: "#fff" }}>
            Stop Simulator
          </button>
        )}
      </div>
      
      <p>{msg}</p>

      <div className="docs">
        <h3>Interactive Controls</h3>
        <ul>
          <li><strong>Arrow keys:</strong> swipe left/right/up/down</li>
          <li><strong>Space/Enter:</strong> tap center</li>
          <li><strong>PageUp/PageDown:</strong> physical side buttons</li>
          <li><strong>Esc:</strong> Home button</li>
          <li><strong>r:</strong> Hardware Refresh</li>
          <li><strong>Mouse click:</strong> tap at cursor position</li>
        </ul>
        <p>
           Note: The simulator mounts the local <code>sim_data</code> directory as the SD card.
           Drop EPUBs or fonts there to read them!
        </p>
      </div>
    </main>
  );
}

export default App;
