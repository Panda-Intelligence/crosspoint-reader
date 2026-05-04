const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

let simProcess = null;

app.post('/api/start', (req, res) => {
    if (simProcess) {
        return res.json({ status: 'already_running' });
    }
    
    // We launch the bin/mofei-sim script.
    // However, the native simulator uses SDL and draws its own window.
    // For a fully integrated Tauri/Web UI we would need to capture the framebuffer or use WebSockets.
    // For now, this is a launch bridge.
    const simPath = path.resolve(__dirname, '../../bin/mofei-sim');
    simProcess = spawn(simPath, [], { stdio: 'inherit' });
    
    simProcess.on('close', () => {
        simProcess = null;
    });
    
    res.json({ status: 'started' });
});

app.post('/api/stop', (req, res) => {
    if (simProcess) {
        simProcess.kill();
        simProcess = null;
    }
    res.json({ status: 'stopped' });
});

app.listen(3001, () => {
    console.log('Bridge server running on port 3001');
});
