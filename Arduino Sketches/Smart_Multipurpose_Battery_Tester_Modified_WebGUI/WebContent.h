#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

// ========================================= WEB CONTENT ========================================
// HTML, CSS, and JavaScript stored in PROGMEM (Flash memory)
// Uses custom lightweight chart - no external dependencies

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Battery Tester</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            padding: 10px;
        }
        .container { max-width: 800px; margin: 0 auto; }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 15px;
            background: #16213e;
            border-radius: 10px;
            margin-bottom: 15px;
        }
        .logo { font-size: 1.3em; font-weight: bold; color: #4ecca3; }
        .wifi-info { text-align: right; font-size: 0.85em; color: #888; }
        .wifi-info .ip { color: #4ecca3; font-family: monospace; }
        .connection-status {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 5px;
        }
        .connection-status.connected { background: #4ecca3; }
        .connection-status.disconnected { background: #e74c3c; }

        .mode-buttons {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-bottom: 15px;
        }
        .mode-btn {
            padding: 12px 8px;
            border: none;
            border-radius: 8px;
            font-size: 0.9em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
            background: #16213e;
            color: #eee;
        }
        .mode-btn:hover { background: #1f3460; }
        .mode-btn:disabled { opacity: 0.5; cursor: not-allowed; }
        .mode-btn.selected { background: #2a4a80; border: 2px solid #4ecca3; }
        .mode-btn.active { background: #4ecca3; color: #1a1a2e; }
        .mode-btn.charge { border-left: 3px solid #2ecc71; }
        .mode-btn.discharge { border-left: 3px solid #e74c3c; }
        .mode-btn.analyze { border-left: 3px solid #3498db; }
        .mode-btn.ir { border-left: 3px solid #f39c12; }

        .card {
            background: #16213e;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 15px;
        }
        .card-title {
            font-size: 0.85em;
            color: #888;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .status-display { text-align: center; padding: 10px 0; }
        .status-mode { font-size: 1.5em; font-weight: bold; color: #4ecca3; margin-bottom: 5px; }
        .status-state { font-size: 0.9em; color: #888; }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
        }
        .stat-item {
            text-align: center;
            padding: 10px;
            background: #1a1a2e;
            border-radius: 8px;
        }
        .stat-value { font-size: 1.8em; font-weight: bold; color: #4ecca3; }
        .stat-label { font-size: 0.8em; color: #888; margin-top: 5px; }
        .stat-value.voltage { color: #3498db; }
        .stat-value.current { color: #e74c3c; }
        .stat-value.capacity { color: #2ecc71; }
        .stat-value.time { color: #f39c12; }
        .stat-value.ir { color: #9b59b6; }

        .chart-container {
            position: relative;
            height: 250px;
            background: #1a1a2e;
            border-radius: 8px;
        }
        #chart { width: 100%; height: 100%; }

        .settings-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 10px;
        }
        .settings-label { font-size: 0.9em; }
        .settings-input { display: flex; align-items: center; gap: 10px; }
        .settings-input input, .settings-input select {
            padding: 8px;
            border: none;
            border-radius: 5px;
            background: #1a1a2e;
            color: #eee;
            font-size: 1em;
        }
        .settings-input input { width: 80px; text-align: center; }

        .control-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 15px;
        }
        .start-btn {
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 1.1em;
            font-weight: bold;
            cursor: pointer;
            background: #2ecc71;
            color: white;
            transition: all 0.2s;
        }
        .start-btn:hover { background: #27ae60; }
        .start-btn:disabled { opacity: 0.3; cursor: not-allowed; }

        .stop-btn {
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 1.1em;
            font-weight: bold;
            cursor: pointer;
            background: #e74c3c;
            color: white;
            transition: all 0.2s;
        }
        .stop-btn:hover { background: #c0392b; }
        .stop-btn:disabled { opacity: 0.3; cursor: not-allowed; }

        .wifi-panel { display: none; }
        .wifi-panel.show { display: block; }
        .wifi-btn {
            padding: 8px 15px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            background: #1f3460;
            color: #eee;
            font-size: 0.85em;
        }
        .wifi-btn:hover { background: #2a4a80; }

        .input-group { margin-bottom: 10px; }
        .input-group label { display: block; margin-bottom: 5px; font-size: 0.85em; color: #888; }
        .input-group input {
            width: 100%;
            padding: 10px;
            border: none;
            border-radius: 5px;
            background: #1a1a2e;
            color: #eee;
        }
        .submit-btn {
            width: 100%;
            padding: 10px;
            border: none;
            border-radius: 5px;
            background: #4ecca3;
            color: #1a1a2e;
            font-weight: bold;
            cursor: pointer;
            margin-top: 10px;
        }
        .submit-btn:hover { background: #3dbb94; }

        .chart-legend {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-top: 10px;
            font-size: 0.85em;
        }
        .legend-item { display: flex; align-items: center; gap: 5px; }
        .legend-color { width: 20px; height: 3px; border-radius: 2px; }
        .legend-voltage { background: #3498db; }
        .legend-current { background: #e74c3c; }

        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 26px;
        }
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #333;
            transition: 0.3s;
            border-radius: 26px;
        }
        .toggle-slider:before {
            position: absolute;
            content: "";
            height: 20px;
            width: 20px;
            left: 3px;
            bottom: 3px;
            background-color: #888;
            transition: 0.3s;
            border-radius: 50%;
        }
        input:checked + .toggle-slider {
            background-color: #4ecca3;
        }
        input:checked + .toggle-slider:before {
            transform: translateX(24px);
            background-color: white;
        }
        .stage-section {
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px solid #333;
        }
        .stage-title {
            margin-bottom: 10px;
            font-weight: bold;
        }
        .stage-title.stage1 { color: #4ecca3; }
        .stage-title.stage2 { color: #3498db; }

        @media (max-width: 500px) {
            .mode-buttons { grid-template-columns: repeat(2, 1fr); }
            .stat-value { font-size: 1.4em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="logo">Battery Tester</div>
            <div class="wifi-info">
                <span class="connection-status disconnected" id="wsStatus"></span>
                <span id="wifiName">Connecting...</span><br>
                <span class="ip" id="ipAddress">---.---.---.---</span>
                <button class="wifi-btn" onclick="toggleWifiPanel()">WiFi</button>
            </div>
        </header>

        <div class="mode-buttons">
            <button class="mode-btn charge" onclick="selectMode('charge')" id="btnCharge">Charge</button>
            <button class="mode-btn discharge" onclick="selectMode('discharge')" id="btnDischarge">Discharge</button>
            <button class="mode-btn analyze" onclick="selectMode('analyze')" id="btnAnalyze">Analyze</button>
            <button class="mode-btn ir" onclick="selectMode('ir')" id="btnIR">IR Test</button>
            <button class="mode-btn" onclick="selectMode('batcheck')" id="btnBatCheck" style="border-left: 3px solid #9b59b6;">Bat Check</button>
            <button class="mode-btn" onclick="selectMode('storage')" id="btnStorage" style="border-left: 3px solid #1abc9c;">Storage</button>
        </div>

        <div class="card" id="dischargeSettings" style="display:none;">
            <div class="card-title">Discharge Settings</div>
            <div class="settings-row">
                <span class="settings-label">Cutoff Voltage</span>
                <div class="settings-input">
                    <input type="number" id="cutoffVoltage" value="3.0" min="2.8" max="3.2" step="0.1"> V
                </div>
            </div>
            <div class="settings-row">
                <span class="settings-label">Discharge Current</span>
                <div class="settings-input">
                    <select id="dischargeCurrent" onchange="checkHighCurrent()">
                        <option value="100">100 mA</option>
                        <option value="200">200 mA</option>
                        <option value="300">300 mA</option>
                        <option value="400">400 mA</option>
                        <option value="500" selected>500 mA</option>
                        <option value="600">600 mA</option>
                        <option value="700">700 mA</option>
                        <option value="800">800 mA</option>
                        <option value="900">900 mA</option>
                        <option value="1000">1000 mA</option>
                        <option value="1500">1500 mA ⚠</option>
                        <option value="2000">2000 mA ⚠</option>
                    </select>
                </div>
            </div>
            <div id="highCurrentWarning" style="display:none; background:#e74c3c; color:white; padding:8px; border-radius:5px; margin-top:10px; text-align:center;">
                ⚠ HIGH CURRENT - May cause MOSFET overheating! Ensure adequate cooling.
            </div>
        </div>

        <div class="card" id="analyzeSettings" style="display:none;">
            <div class="card-title">Analyze Settings</div>

            <div class="settings-row">
                <span class="settings-label">Staged Discharge Mode</span>
                <div class="settings-input">
                    <label class="toggle-switch">
                        <input type="checkbox" id="stagedMode" onchange="toggleStagedSettings()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            </div>

            <div id="stagedSettings" style="display:none;">
                <div class="stage-section">
                    <div class="stage-title stage1">Stage 1 (High Current)</div>

                    <div class="settings-row">
                        <span class="settings-label">Discharge Current</span>
                        <div class="settings-input">
                            <select id="stage1Current" onchange="validateStagedSettings()">
                                <option value="300">300 mA</option>
                                <option value="400">400 mA</option>
                                <option value="500" selected>500 mA</option>
                                <option value="600">600 mA</option>
                                <option value="700">700 mA</option>
                                <option value="800">800 mA</option>
                                <option value="900">900 mA</option>
                                <option value="1000">1000 mA</option>
                                <option value="1500">1500 mA ⚠</option>
                                <option value="2000">2000 mA ⚠</option>
                            </select>
                        </div>
                    </div>

                    <div class="settings-row">
                        <span class="settings-label">Transition Voltage</span>
                        <div class="settings-input">
                            <input type="number" id="stage1Transition" value="3.3" min="3.0" max="3.8" step="0.1" onchange="validateStagedSettings()"> V
                        </div>
                    </div>
                </div>

                <div class="stage-section">
                    <div class="stage-title stage2">Stage 2 (Low Current)</div>

                    <div class="settings-row">
                        <span class="settings-label">Discharge Current</span>
                        <div class="settings-input">
                            <select id="stage2Current" onchange="validateStagedSettings()">
                                <option value="100">100 mA</option>
                                <option value="200">200 mA</option>
                                <option value="300" selected>300 mA</option>
                                <option value="400">400 mA</option>
                                <option value="500">500 mA</option>
                                <option value="600">600 mA</option>
                                <option value="700">700 mA</option>
                                <option value="800">800 mA</option>
                                <option value="900">900 mA</option>
                                <option value="1000">1000 mA</option>
                                <option value="1500">1500 mA ⚠</option>
                                <option value="2000">2000 mA ⚠</option>
                            </select>
                        </div>
                    </div>

                    <div class="settings-row">
                        <span class="settings-label">Final Cutoff</span>
                        <div class="settings-input">
                            <input type="number" id="stage2Cutoff" value="3.0" min="2.8" max="3.2" step="0.1" onchange="validateStagedSettings()"> V
                        </div>
                    </div>
                </div>

                <div id="stagedValidationError" style="display:none; background:#e74c3c; color:white; padding:8px; border-radius:5px; margin-top:10px; text-align:center;">
                </div>
                <div id="analyzeHighCurrentWarning" style="display:none; background:#e74c3c; color:white; padding:8px; border-radius:5px; margin-top:10px; text-align:center;">
                    ⚠ HIGH CURRENT - May cause MOSFET overheating! Ensure adequate cooling.
                </div>
            </div>
        </div>

        <div class="card">
            <div class="status-display">
                <div class="status-mode" id="currentMode">IDLE</div>
                <div class="status-state" id="currentState">Select a mode and press Start</div>
            </div>
        </div>

        <div class="card">
            <div class="stats-grid">
                <div class="stat-item">
                    <div class="stat-value voltage" id="voltage">--</div>
                    <div class="stat-label">Voltage (V)</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value current" id="current">--</div>
                    <div class="stat-label">Current (mA)</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value capacity" id="capacity">--</div>
                    <div class="stat-label">Capacity (mAh)</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value time" id="elapsed">--:--:--</div>
                    <div class="stat-label">Elapsed Time</div>
                </div>
                <div class="stat-item" id="irStatItem" style="display:none;">
                    <div class="stat-value ir" id="irValue">--</div>
                    <div class="stat-label">Internal R (mΩ)</div>
                </div>
            </div>
        </div>

        <div class="card">
            <div class="card-title">Voltage & Current</div>
            <div class="chart-container">
                <canvas id="chart"></canvas>
            </div>
            <div class="chart-legend">
                <div class="legend-item"><div class="legend-color legend-voltage"></div>Voltage (V)</div>
                <div class="legend-item"><div class="legend-color legend-current"></div>Current (mA)</div>
            </div>
        </div>

        <div class="control-buttons">
            <button class="start-btn" onclick="startOperation()" id="startBtn">START</button>
            <button class="stop-btn" onclick="stopOperation()" id="stopBtn" disabled>STOP</button>
        </div>

        <div class="card wifi-panel" id="wifiPanel">
            <div class="card-title">WiFi Configuration</div>
            <div id="wifiStatus" style="margin-bottom: 15px; padding: 10px; background: #1a1a2e; border-radius: 5px;">
                <div style="margin-bottom: 8px;">
                    <span style="color: #888;">Access Point:</span>
                    <span id="apInfo" style="color: #4ecca3;">--</span>
                </div>
                <div>
                    <span style="color: #888;">Network:</span>
                    <span id="staInfo" style="color: #888;">Not connected</span>
                </div>
            </div>
            <div class="input-group">
                <label>Network Name (SSID)</label>
                <input type="text" id="wifiSSID" placeholder="Enter WiFi name">
            </div>
            <div class="input-group">
                <label>Password</label>
                <input type="password" id="wifiPassword" placeholder="Enter password">
            </div>
            <button class="submit-btn" onclick="saveWifiConfig()" id="wifiConnectBtn">Connect to Network</button>
            <button class="submit-btn" onclick="disconnectWifi()" id="wifiDisconnectBtn" style="display:none; background:#e74c3c; margin-top:5px;">Disconnect from Network</button>
            <button class="submit-btn" onclick="forgetWifi()" id="wifiForgetBtn" style="display:none; background:#95a5a6; margin-top:5px;">Forget Network (Clear Saved)</button>
        </div>
    </div>

    <script>
        let ws = null;
        let isRunning = false;
        let selectedMode = 'charge';  // Default selected mode

        const voltageData = [];
        const currentData = [];
        const timeData = [];
        const maxPoints = 3600;
        const voltageMin = 2.5, voltageMax = 4.5;
        const currentMin = 0, currentMax = 1100;

        function drawChart() {
            const canvas = document.getElementById('chart');
            const ctx = canvas.getContext('2d');
            const rect = canvas.parentElement.getBoundingClientRect();
            canvas.width = rect.width;
            canvas.height = rect.height;

            const w = canvas.width, h = canvas.height;
            const padding = { top: 20, right: 50, bottom: 30, left: 50 };
            const chartW = w - padding.left - padding.right;
            const chartH = h - padding.top - padding.bottom;

            ctx.fillStyle = '#1a1a2e';
            ctx.fillRect(0, 0, w, h);

            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 4; i++) {
                const y = padding.top + (chartH / 4) * i;
                ctx.beginPath();
                ctx.moveTo(padding.left, y);
                ctx.lineTo(w - padding.right, y);
                ctx.stroke();
            }

            ctx.font = '11px sans-serif';
            ctx.fillStyle = '#3498db';
            for (let i = 0; i <= 4; i++) {
                const v = voltageMax - (voltageMax - voltageMin) * (i / 4);
                const y = padding.top + (chartH / 4) * i;
                ctx.fillText(v.toFixed(1) + 'V', 5, y + 4);
            }

            ctx.fillStyle = '#e74c3c';
            for (let i = 0; i <= 4; i++) {
                const c = currentMax - (currentMax - currentMin) * (i / 4);
                const y = padding.top + (chartH / 4) * i;
                ctx.fillText(c.toFixed(0), w - 45, y + 4);
            }

            ctx.fillStyle = '#888';
            if (timeData.length > 0) {
                const maxTime = Math.max(...timeData);
                for (let i = 0; i <= 4; i++) {
                    const t = (maxTime * i / 4 / 60000).toFixed(0);
                    const x = padding.left + (chartW / 4) * i;
                    ctx.fillText(t + 'm', x - 10, h - 10);
                }
            }

            if (voltageData.length < 2) return;
            const xScale = chartW / (timeData.length - 1 || 1);

            ctx.strokeStyle = '#3498db';
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let i = 0; i < voltageData.length; i++) {
                const x = padding.left + i * xScale;
                const y = padding.top + chartH - ((voltageData[i] - voltageMin) / (voltageMax - voltageMin)) * chartH;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();

            ctx.strokeStyle = '#e74c3c';
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let i = 0; i < currentData.length; i++) {
                const x = padding.left + i * xScale;
                const y = padding.top + chartH - ((currentData[i] - currentMin) / (currentMax - currentMin)) * chartH;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function addDataPoint(data) {
            if (voltageData.length >= maxPoints) {
                voltageData.shift(); currentData.shift(); timeData.shift();
            }
            voltageData.push(data.v);
            currentData.push(data.c);
            timeData.push(data.t);
            drawChart();
        }

        function loadHistory(points) {
            voltageData.length = 0; currentData.length = 0; timeData.length = 0;
            points.forEach(p => {
                voltageData.push(p.v); currentData.push(p.c); timeData.push(p.t);
            });
            drawChart();
        }

        function clearChart() {
            voltageData.length = 0; currentData.length = 0; timeData.length = 0;
            drawChart();
        }

        function connectWebSocket() {
            const host = window.location.hostname;
            ws = new WebSocket('ws://' + host + '/ws');

            ws.onopen = function() {
                document.getElementById('wsStatus').classList.remove('disconnected');
                document.getElementById('wsStatus').classList.add('connected');
                document.getElementById('wifiName').textContent = 'Connected';
                document.getElementById('ipAddress').textContent = host;
            };

            ws.onclose = function() {
                document.getElementById('wsStatus').classList.remove('connected');
                document.getElementById('wsStatus').classList.add('disconnected');
                document.getElementById('wifiName').textContent = 'Disconnected';
                setTimeout(connectWebSocket, 2000);
            };

            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    handleMessage(data);
                } catch (e) { console.error('Parse error:', e); }
            };

            ws.onerror = function(error) { console.error('WebSocket error:', error); };
        }

        function handleMessage(data) {
            if (data.type === 'status') updateStatus(data);
            else if (data.type === 'datapoint') addDataPoint(data);
            else if (data.type === 'history') loadHistory(data.points);
            else if (data.type === 'error') showError(data.message);
            else if (data.type === 'wifi_status') updateWifiStatus(data);
        }

        function updateWifiStatus(data) {
            // Update AP info
            const apInfo = document.getElementById('apInfo');
            if (data.ap_enabled) {
                if (data.ap_disable_in !== undefined && data.ap_disable_in > 0) {
                    apInfo.textContent = data.ap_ssid + ' (' + data.ap_ip + ') - Disabling in ' + data.ap_disable_in + 's';
                    apInfo.style.color = '#f39c12';
                } else {
                    apInfo.textContent = data.ap_ssid + ' (' + data.ap_ip + ')';
                    apInfo.style.color = '#4ecca3';
                }
            } else {
                apInfo.textContent = 'Disabled (connected to network)';
                apInfo.style.color = '#888';
            }

            // Update STA info
            const staInfo = document.getElementById('staInfo');
            const disconnectBtn = document.getElementById('wifiDisconnectBtn');
            const forgetBtn = document.getElementById('wifiForgetBtn');

            if (data.sta_connected) {
                staInfo.textContent = data.sta_ssid + ' (' + data.sta_ip + ')';
                staInfo.style.color = '#2ecc71';
                disconnectBtn.style.display = 'block';
                forgetBtn.style.display = 'block';
            } else {
                if (data.sta_ssid) {
                    staInfo.textContent = 'Not connected (saved: ' + data.sta_ssid + ')';
                    forgetBtn.style.display = 'block';  // Show forget btn if credentials are saved
                } else {
                    staInfo.textContent = 'Not connected';
                    forgetBtn.style.display = 'none';
                }
                staInfo.style.color = '#888';
                disconnectBtn.style.display = 'none';
            }
        }

        function showError(message) {
            // Update status display to show error
            document.getElementById('currentState').textContent = message;
            document.getElementById('currentState').style.color = '#e74c3c';
            // Reset color after 3 seconds
            setTimeout(() => {
                document.getElementById('currentState').style.color = '#888';
            }, 3000);
        }

        function updateStatus(data) {
            const modeNames = {
                'idle': 'IDLE',
                'charge': 'CHARGING',
                'discharge': 'DISCHARGING',
                'analyze_charge': 'ANALYZE (Charging)',
                'analyze_rest': 'ANALYZE (Resting)',
                'analyze_discharge': 'ANALYZE (Discharging)',
                'analyze_discharge_s1': 'ANALYZE (Stage 1)',
                'analyze_discharge_s2': 'ANALYZE (Stage 2)',
                'ir': 'IR TEST',
                'complete': 'COMPLETE'
            };

            document.getElementById('currentMode').textContent = modeNames[data.mode] || data.mode;
            document.getElementById('currentState').textContent = data.status || 'Ready';
            document.getElementById('voltage').textContent = data.voltage ? data.voltage.toFixed(2) : '--';
            document.getElementById('current').textContent = data.current || '0';
            document.getElementById('capacity').textContent = data.capacity ? data.capacity.toFixed(1) : '0';
            document.getElementById('elapsed').textContent = data.time || '00:00:00';

            // Hide non-essential stats for Battery Check and Storage modes
            const isBatCheckOrStorage = data.mode === 'batcheck' || data.mode === 'storage';
            document.getElementById('current').parentElement.style.display = isBatCheckOrStorage ? 'none' : 'block';
            document.getElementById('capacity').parentElement.style.display = isBatCheckOrStorage ? 'none' : 'block';
            document.getElementById('elapsed').parentElement.style.display = isBatCheckOrStorage ? 'none' : 'block';

            // Show IR result when available (persists until new operation starts)
            if (data.ir !== undefined) {
                document.getElementById('irValue').textContent = data.ir.toFixed(0);
                document.getElementById('irStatItem').style.display = 'block';
            }

            isRunning = data.mode !== 'idle' && data.mode !== 'complete';

            // Update button states
            document.getElementById('startBtn').disabled = isRunning;
            document.getElementById('stopBtn').disabled = !isRunning;

            // Disable mode selection while running
            ['btnCharge', 'btnDischarge', 'btnAnalyze', 'btnIR', 'btnBatCheck', 'btnStorage'].forEach(id => {
                document.getElementById(id).disabled = isRunning;
            });

            // Show active mode from device
            document.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
            // Check analyze first since analyze_charge/analyze_discharge contain those words
            if (data.mode.startsWith('analyze')) {
                document.getElementById('btnAnalyze').classList.add('active');
            } else if (data.mode === 'charge') {
                document.getElementById('btnCharge').classList.add('active');
            } else if (data.mode === 'discharge') {
                document.getElementById('btnDischarge').classList.add('active');
            } else if (data.mode === 'ir') {
                document.getElementById('btnIR').classList.add('active');
            } else if (data.mode === 'batcheck') {
                document.getElementById('btnBatCheck').classList.add('active');
            } else if (data.mode === 'storage') {
                document.getElementById('btnStorage').classList.add('active');
            }
        }

        function sendCommand(cmd) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify(cmd));
            }
        }

        // Select mode (doesn't start it)
        function selectMode(mode) {
            selectedMode = mode;

            // Update visual selection
            document.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('selected'));
            if (mode === 'charge') document.getElementById('btnCharge').classList.add('selected');
            else if (mode === 'discharge') document.getElementById('btnDischarge').classList.add('selected');
            else if (mode === 'analyze') document.getElementById('btnAnalyze').classList.add('selected');
            else if (mode === 'ir') document.getElementById('btnIR').classList.add('selected');
            else if (mode === 'batcheck') document.getElementById('btnBatCheck').classList.add('selected');
            else if (mode === 'storage') document.getElementById('btnStorage').classList.add('selected');

            // Show/hide settings panels
            document.getElementById('dischargeSettings').style.display =
                (mode === 'discharge') ? 'block' : 'none';
            document.getElementById('analyzeSettings').style.display =
                (mode === 'analyze') ? 'block' : 'none';
        }

        // Start the selected operation
        function startOperation() {
            clearChart();
            // Hide previous IR result when starting new operation
            document.getElementById('irStatItem').style.display = 'none';

            if (selectedMode === 'discharge') {
                const cutoff = parseFloat(document.getElementById('cutoffVoltage').value);
                const current = parseInt(document.getElementById('dischargeCurrent').value);
                sendCommand({ cmd: 'start_discharge', cutoff: cutoff, current: current });
            } else if (selectedMode === 'analyze') {
                const staged = document.getElementById('stagedMode').checked;
                if (staged) {
                    if (!validateStagedSettings()) {
                        return;  // Don't start if validation fails
                    }
                    sendCommand({
                        cmd: 'start_analyze',
                        staged: true,
                        stage1_current: parseInt(document.getElementById('stage1Current').value),
                        stage1_transition: parseFloat(document.getElementById('stage1Transition').value),
                        stage2_current: parseInt(document.getElementById('stage2Current').value),
                        stage2_cutoff: parseFloat(document.getElementById('stage2Cutoff').value)
                    });
                } else {
                    sendCommand({ cmd: 'start_analyze' });
                }
            } else {
                sendCommand({ cmd: 'start_' + selectedMode });
            }
        }

        // Stop/abort the current operation
        function stopOperation() {
            sendCommand({ cmd: 'abort' });
        }

        function toggleWifiPanel() {
            document.getElementById('wifiPanel').classList.toggle('show');
        }

        function saveWifiConfig() {
            const ssid = document.getElementById('wifiSSID').value;
            const password = document.getElementById('wifiPassword').value;

            if (!ssid) {
                alert('Please enter a network name (SSID)');
                return;
            }

            // Show connecting status
            document.getElementById('staInfo').textContent = 'Connecting to ' + ssid + '...';
            document.getElementById('staInfo').style.color = '#f39c12';

            sendCommand({ cmd: 'wifi_config', ssid: ssid, password: password });
        }

        function disconnectWifi() {
            sendCommand({ cmd: 'wifi_disconnect' });
        }

        function forgetWifi() {
            if (confirm('Clear saved WiFi credentials? The device will no longer auto-connect to this network on boot.')) {
                sendCommand({ cmd: 'wifi_forget' });
            }
        }

        function checkHighCurrent() {
            const current = parseInt(document.getElementById('dischargeCurrent').value);
            const warning = document.getElementById('highCurrentWarning');
            warning.style.display = (current > 1000) ? 'block' : 'none';
        }

        function toggleStagedSettings() {
            const staged = document.getElementById('stagedMode').checked;
            document.getElementById('stagedSettings').style.display = staged ? 'block' : 'none';
            if (staged) {
                validateStagedSettings();
            }
        }

        function validateStagedSettings() {
            const s1Current = parseInt(document.getElementById('stage1Current').value);
            const s1Transition = parseFloat(document.getElementById('stage1Transition').value);
            const s2Current = parseInt(document.getElementById('stage2Current').value);
            const s2Cutoff = parseFloat(document.getElementById('stage2Cutoff').value);

            const errorDiv = document.getElementById('stagedValidationError');
            let errors = [];

            // Validate: Stage 2 current must be <= Stage 1 current
            if (s2Current > s1Current) {
                errors.push('Stage 2 current must be <= Stage 1');
            }

            // Validate: Transition voltage must be > final cutoff
            if (s1Transition <= s2Cutoff) {
                errors.push('Transition V must be > Final cutoff');
            }

            // Update Stage 2 current options dynamically
            const s2Select = document.getElementById('stage2Current');
            Array.from(s2Select.options).forEach(opt => {
                opt.disabled = parseInt(opt.value) > s1Current;
            });

            // Update transition voltage min based on cutoff
            document.getElementById('stage1Transition').min = (s2Cutoff + 0.1).toFixed(1);

            // Update cutoff max based on transition
            document.getElementById('stage2Cutoff').max = (s1Transition - 0.1).toFixed(1);

            // Show high current warning if either stage exceeds 1000mA
            const highCurrentWarning = document.getElementById('analyzeHighCurrentWarning');
            highCurrentWarning.style.display = (s1Current > 1000 || s2Current > 1000) ? 'block' : 'none';

            if (errors.length > 0) {
                errorDiv.textContent = errors.join('. ');
                errorDiv.style.display = 'block';
                return false;
            } else {
                errorDiv.style.display = 'none';
                return true;
            }
        }

        window.addEventListener('load', function() {
            drawChart();
            connectWebSocket();
            selectMode('charge');  // Default selection
        });

        window.addEventListener('resize', drawChart);
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_CONTENT_H
