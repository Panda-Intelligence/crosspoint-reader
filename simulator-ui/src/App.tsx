import { useState } from 'react'
import './App.css'

function App() {
  const [running, setRunning] = useState(false)

  const startSim = async () => {
    try {
      await fetch('http://localhost:3001/api/start', { method: 'POST' })
      setRunning(true)
    } catch (e) {
      alert("Failed to start simulator")
    }
  }

  const stopSim = async () => {
    try {
      await fetch('http://localhost:3001/api/stop', { method: 'POST' })
      setRunning(false)
    } catch (e) {
      alert("Failed to stop simulator")
    }
  }

  return (
    <div className="App">
      <h1>Mofei Simulator Control Panel</h1>
      <div className="card">
        {!running ? (
          <button onClick={startSim}>Launch Native Simulator</button>
        ) : (
          <button onClick={stopSim}>Stop Simulator</button>
        )}
      </div>
      <p className="read-the-docs">
        Note: The simulator will open in a separate SDL window.
        Use arrow keys to swipe, space to tap.
      </p>
    </div>
  )
}

export default App
