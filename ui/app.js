/**
 * AeroFirmware Ground Station — app.js
 * Professional real-time avionics monitor UI
 *
 * Features:
 *  - Web Serial API for direct UART connection
 *  - Binary telemetry frame decoder (CRC-16)
 *  - ASCII NMEA-style frame parser ($AERO,...)
 *  - Artificial Horizon Indicator (AHI) canvas renderer
 *  - Live time-series chart (rolling 30s window)
 *  - EKF state table renderer
 *  - Flash workflow simulation (STM32/ESP32)
 */

'use strict';

/* ============================================================
 * Global state
 * ============================================================ */
const state = {
    connected: false,
    streaming: false,
    port: null,
    reader: null,
    selectedFile: null,
    tab: 'flash',
    roll: 0, pitch: 0, yaw: 0,
    pos: [0, 0, 0],
    vel: [0, 0, 0],
    quat: [1, 0, 0, 0],
    accel_bias: [0, 0, 0],
    gyro_bias: [0, 0, 0],
    P_diag: new Array(15).fill(0),
    updates: 0,
    innov: { baro: 0, gpsN: 0, gpsE: 0, magX: 0, NIS_baro: 0, NIS_gps: 0 },
    sensors: {},
    tick_ms: 0,
    free_heap: 0,
    lastRateTime: Date.now(),
    frameCount: 0,
    fps: 0,
    map: null,
    marker: null,
    path: null,
    pathCoords: [],
};

/* Rolling chart data */
const CHART_LEN = 300;   // 30s @ 10 Hz display
const chartData = {
    labels: [], channels: {
        alt: [], roll: [], pitch: [], yaw: [],
        temp: [], co: [], pressure: []
    }
};
for (let k in chartData.channels) {
    for (let i = 0; i < CHART_LEN; i++) chartData.channels[k].push(0);
    chartData.labels.push('');
}

/* ============================================================
 * Tab switching
 * ============================================================ */
function switchTab(name) {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    document.getElementById(`tab-${name}`).classList.add('active');
    document.getElementById(`btn-${name}`)?.classList.add('active');
    state.tab = name;
}

/* ============================================================
 * Connection
 * ============================================================ */
async function connectDevice() {
    const btn = document.getElementById('btn-connect');

    if (state.connected) {
        await disconnectDevice();
        return;
    }

    try {
        if ('serial' in navigator) {
            // Real Web Serial API
            state.port = await navigator.serial.requestPort();
            const baud = parseInt(document.getElementById('baud-select').value);
            await state.port.open({ baudRate: baud });
            appendLog('[INFO] Serial port opened', 'info');
            state.connected = true;
            startSerialRead();
        } else {
            // Demo mode (no Web Serial)
            appendLog('[WARN] Web Serial API not available — running in DEMO mode', 'warn');
            appendLog('[INFO] Connecting to demo data generator...', 'info');
            state.connected = true;
            startDemoMode();
        }

        updateConnStatus(true);
        btn.textContent = 'Disconnect';
        showDeviceInfo();
        document.getElementById('device-actions').style.display = 'block';
        appendLog('[OK] Device connected.', 'info');
        document.getElementById('btn-flash-action').disabled = false;

    } catch (err) {
        appendLog(`[ERR] Connection failed: ${err.message}`, 'err');
    }
}

async function disconnectDevice() {
    state.connected = false;
    state.streaming = false;
    if (state.reader) { try { await state.reader.cancel(); } catch (e) { } }
    if (state.port) { try { await state.port.close(); } catch (e) { } }
    updateConnStatus(false);
    document.getElementById('btn-connect').textContent = 'Connect Device';
    document.getElementById('device-actions').style.display = 'none';
    appendLog('[INFO] Device disconnected.', 'info');
}

function updateConnStatus(connected) {
    const dot = document.getElementById('conn-dot');
    const label = document.getElementById('conn-label');
    dot.className = connected ? 'status-dot connected' : 'status-dot';
    label.textContent = connected ? 'Connected' : 'Disconnected';
}

function showDeviceInfo() {
    const mcu = document.getElementById('mcu-select').value;
    const info = {
        'STM32H7': ['STM32H743ZI', '2048 KB', '1024 KB'],
        'STM32F4': ['STM32F407VG', '1024 KB', '192 KB'],
        'STM32F7': ['STM32F767ZI', '2048 KB', '512 KB'],
        'STM32L4': ['STM32L476RG', '1024 KB', '128 KB'],
        'STM32L0': ['STM32L073RZ', '192 KB', '20 KB'],
        'ESP32': ['ESP32-D0WD', '4 MB', '520 KB'],
        'ESP32_DEVKITV1': ['ESP32-D0WD', '4 MB', '520 KB'],
        'ESP32_DEV_MODULE': ['ESP32-D0WD', '4 MB', '520 KB'],
        'ESP32-S2': ['ESP32-S2', '4 MB', '320 KB'],
        'ESP32-S3': ['ESP32-S3', '8 MB', '512 KB'],
        'ESP32-C3': ['ESP32-C3', '4 MB', '400 KB'],
        'ESP32-C6': ['ESP32-C6', '4 MB', '512 KB'],
        'ESP32-H2': ['ESP32-H2', '2 MB', '320 KB'],
        'ESP8266': ['ESP8266EX', '4 MB', '80 KB'],
    }[mcu] || ['—', '—', '—'];

    document.getElementById('di-mcu').textContent = info[0];
    document.getElementById('di-flash').textContent = info[1];
    document.getElementById('di-ram').textContent = info[2];
    document.getElementById('di-fwver').textContent = '1.0.0';
    document.getElementById('device-info').style.display = 'block';
}

/* ============================================================
 * Serial read (Web Serial API)
 * ============================================================ */
async function startSerialRead() {
    state.streaming = true;
    const dot = document.getElementById('conn-dot');
    dot.className = 'status-dot streaming';

    const decoder = new TextDecoder();
    let lineBuffer = '';

    while (state.connected) {
        try {
            state.reader = state.port.readable.getReader();
            while (true) {
                const { done, value } = await state.reader.read();
                if (done) break;
                lineBuffer += decoder.decode(value, { stream: true });
                const lines = lineBuffer.split('\n');
                lineBuffer = lines.pop();
                for (const line of lines) {
                    if (line.startsWith('$AERO,')) parseAsciiFrame(line.trim());
                }
            }
        } catch (e) {
            if (state.connected) appendLog(`[WARN] Serial error: ${e.message}`, 'warn');
        } finally {
            state.reader?.releaseLock();
        }
        if (!state.connected) break;
        await new Promise(r => setTimeout(r, 100));
    }
}

/* ============================================================
 * ASCII frame parser: Enhanced with wireless data support
 * ============================================================ */
function parseAsciiFrame(line) {
    // Use the enhanced telemetry parser that handles wireless data
    parseTelemetryLine(line.trim());
}

/* ============================================================
 * Demo mode — generates realistic avionics simulation data
 * ============================================================ */
let demoT = 0;
let demoTimer = null;

function startDemoMode() {
    if (demoTimer) clearInterval(demoTimer);
    demoTimer = setInterval(demoTick, 50); // 20 Hz
    const dot = document.getElementById('conn-dot');
    dot.className = 'status-dot streaming';
    document.getElementById('conn-label').textContent = 'Demo Mode';
}

function demoTick() {
    if (!state.connected) { clearInterval(demoTimer); return; }
    demoT += 0.05;

    /* Simulated flight data */
    const roll = 15 * Math.sin(demoT * 0.5) + 2 * Math.sin(demoT * 3.1);
    const pitch = 5 * Math.sin(demoT * 0.3) + Math.cos(demoT);
    const yaw = (demoT * 8) % 360;
    const alt = 100 + 50 * Math.sin(demoT * 0.2) + demoT * 0.5;
    const lat = 12.9716 + demoT * 0.00001;
    const lon = 77.5946 + demoT * 0.00001;
    const sats = 12;
    const co = 2 + 1.5 * Math.sin(demoT * 0.1);

    /* Simulated sensor data */
    const pressure = 1013.25 * Math.pow(1 - alt / 44330, 5.255);

    updateSensorDisplay({
        ax: 9.81 * Math.sin(pitch * Math.PI / 180),
        ay: -9.81 * Math.sin(roll * Math.PI / 180),
        az: 9.81 * Math.cos(pitch * Math.PI / 180) * Math.cos(roll * Math.PI / 180),
        gx: 0.05 * Math.sin(demoT), gy: 0.03 * Math.cos(demoT), gz: 0.01,
        pressure, alt_baro: alt, temp_baro: 25 - alt * 0.0065,
        temp_env: 23.5 + 0.3 * Math.sin(demoT * 0.05),
        hum_env: 55 + 5 * Math.sin(demoT * 0.04),
        mx: 0.22 * Math.cos(yaw * Math.PI / 180), my: 0.22 * Math.sin(yaw * Math.PI / 180), mz: -0.42,
        lat, lon, alt_gnss: alt, speed: 12 + 2 * Math.sin(demoT), sats, hdop: 1.2,
        co, co_mv: 300 + co * 3,
    });

    ingestTelemetry({
        tick: Math.round(demoT * 1000),
        roll, pitch, yaw, alt, lat, lon, sats, co,
        /* Simulated Control Data */
        roll_sp: 10 * Math.sin(demoT * 0.4),
        pitch_sp: 5 * Math.cos(demoT * 0.3),
        roll_act: Math.sin(demoT * 2.0) * 0.4 + (roll / 30),
        pitch_act: Math.cos(demoT * 1.5) * 0.3 + (pitch / 20)
    });

    /* EKF state simulation */
    updateEKFState({
        pos: [demoT * 0.5, demoT * 0.3, -alt],
        vel: [12 * Math.cos(yaw * Math.PI / 180), 12 * Math.sin(yaw * Math.PI / 180), 0],
        quat: eulerToQuat(roll, pitch, yaw),
        accel_bias: [0.001, -0.002, 0.003],
        gyro_bias: [0.0001, -0.0002, 0.0001],
        P_diag: [1, 1, 0.5, 0.1, 0.1, 0.05, 0.01, 0.01, 0.01, 0.01, 0.001, 0.001, 0.001, 0.0001, 0.0001],
        updates: Math.round(demoT * 200),
        innov: {
            baro: (Math.random() - 0.5) * 0.3, gpsN: (Math.random() - 0.5) * 1, gpsE: (Math.random() - 0.5) * 1,
            magX: (Math.random() - 0.5) * 0.01, NIS_baro: Math.random() * 2, NIS_gps: Math.random() * 8
        },
        roll, pitch, yaw,
        free_heap: 48000 - Math.round(demoT * 10)
    });

    state.tick_ms = Math.round(demoT * 1000);
    state.free_heap = 48000;
}

function eulerToQuat(roll_deg, pitch_deg, yaw_deg) {
    const r = roll_deg * Math.PI / 180 / 2;
    const p = pitch_deg * Math.PI / 180 / 2;
    const y = yaw_deg * Math.PI / 180 / 2;
    return [
        Math.cos(r) * Math.cos(p) * Math.cos(y) + Math.sin(r) * Math.sin(p) * Math.sin(y),
        Math.sin(r) * Math.cos(p) * Math.cos(y) - Math.cos(r) * Math.sin(p) * Math.sin(y),
        Math.cos(r) * Math.sin(p) * Math.cos(y) + Math.sin(r) * Math.cos(p) * Math.sin(y),
        Math.cos(r) * Math.cos(p) * Math.sin(y) - Math.sin(r) * Math.sin(p) * Math.cos(y),
    ];
}

/* ============================================================
 * Wireless Communication Data Processing
 * ============================================================ */

// Process wireless status updates from firmware
function processWirelessStatus(data) {
    const parts = data.split(',');
    if (parts.length < 3) return;
    
    const type = parts[0]; // WIFI, BT, TELEMETRY
    const status = parts[1];
    const values = parts.slice(2);
    
    switch (type) {
        case 'WIFI':
            processWifiStatus(status, values);
            break;
        case 'BT':
            processBtStatus(status, values);
            break;
        case 'TELEMETRY':
            processTelemetryStatus(status, values);
            break;
    }
}

// Process WiFi status updates
function processWifiStatus(status, values) {
    const dot = document.getElementById('wifi-status-dot');
    const label = document.getElementById('wifi-status-label');
    const info = document.getElementById('wifi-info');
    const ip = document.getElementById('wifi-ip');
    const rssi = document.getElementById('wifi-rssi');
    
    // Update header WiFi icon
    const wifiIcon = document.getElementById('wifi-status-icon');
    wifiIcon.className = 'wireless-icon wifi-icon';
    
    dot.className = 'status-dot';
    
    switch (status) {
        case 'OFF':
            dot.classList.add('off');
            label.textContent = 'Off';
            info.style.display = 'none';
            wifiIcon.classList.add('off');
            wifiIcon.setAttribute('title', 'WiFi: Off');
            break;
        case 'CONNECTING':
            dot.classList.add('connecting');
            label.textContent = 'Connecting...';
            info.style.display = 'none';
            wifiIcon.classList.add('connecting');
            wifiIcon.setAttribute('title', 'WiFi: Connecting...');
            break;
        case 'CONNECTED':
            dot.classList.add('healthy');
            label.textContent = 'Connected';
            info.style.display = 'block';
            if (values[0]) ip.textContent = values[0];
            if (values[1]) rssi.textContent = values[1] + ' dBm';
            wifiIcon.classList.add('connected');
            wifiIcon.setAttribute('title', `WiFi: Connected (${values[0] || 'No IP'})`);
            break;
        case 'AP_ACTIVE':
            dot.classList.add('healthy');
            label.textContent = 'AP Active';
            info.style.display = 'block';
            if (values[0]) ip.textContent = values[0];
            if (values[1]) rssi.textContent = 'N/A';
            wifiIcon.classList.add('connected');
            wifiIcon.setAttribute('title', `WiFi: AP Active (${values[0] || 'No IP'})`);
            break;
        case 'ERROR':
            dot.classList.add('error');
            label.textContent = 'Error';
            info.style.display = 'none';
            wifiIcon.classList.add('error');
            wifiIcon.setAttribute('title', 'WiFi: Error');
            break;
    }
}

// Process Bluetooth status updates
function processBtStatus(status, values) {
    const dot = document.getElementById('bt-status-dot');
    const label = document.getElementById('bt-status-label');
    const info = document.getElementById('bt-info');
    const device = document.getElementById('bt-device');
    const rssi = document.getElementById('bt-rssi');
    
    // Update header Bluetooth icon
    const btIcon = document.getElementById('bt-status-icon');
    btIcon.className = 'wireless-icon bt-icon';
    
    dot.className = 'status-dot';
    
    switch (status) {
        case 'OFF':
            dot.classList.add('off');
            label.textContent = 'Off';
            info.style.display = 'none';
            btIcon.classList.add('off');
            btIcon.setAttribute('title', 'Bluetooth: Off');
            break;
        case 'DISCOVERABLE':
            dot.classList.add('connecting');
            label.textContent = 'Discoverable';
            info.style.display = 'none';
            btIcon.classList.add('discoverable');
            btIcon.setAttribute('title', 'Bluetooth: Discoverable');
            break;
        case 'CONNECTED':
            dot.classList.add('healthy');
            label.textContent = 'Connected';
            info.style.display = 'block';
            if (values[0]) device.textContent = values[0];
            if (values[1]) rssi.textContent = values[1] + ' dBm';
            btIcon.classList.add('connected');
            btIcon.setAttribute('title', `Bluetooth: Connected (${values[0] || 'Unknown Device'})`);
            break;
        case 'SCANNING':
            dot.classList.add('connecting');
            label.textContent = 'Scanning...';
            info.style.display = 'none';
            btIcon.classList.add('scanning');
            btIcon.setAttribute('title', 'Bluetooth: Scanning...');
            break;
        case 'ERROR':
            dot.classList.add('error');
            label.textContent = 'Error';
            info.style.display = 'none';
            btIcon.classList.add('error');
            btIcon.setAttribute('title', 'Bluetooth: Error');
            break;
    }
}

// Process telemetry status updates
function processTelemetryStatus(status, values) {
    const rate = document.getElementById('telemetry-rate');
    const bytes = document.getElementById('telemetry-bytes');
    const mode = document.getElementById('telemetry-mode');
    
    if (values[0]) rate.textContent = values[0] + ' Hz';
    if (values[1]) bytes.textContent = values[1] + ' KB/s';
    
    // Update mode based on active connection
    if (status === 'ACTIVE') {
        if (values[2]) mode.value = values[2].toLowerCase();
    }
}

// Process discovered Bluetooth devices
function processBtDevices(devices) {
    const deviceList = document.getElementById('bt-devices');
    if (!deviceList) return;
    
    deviceList.innerHTML = '';
    
    devices.forEach(device => {
        const deviceEl = document.createElement('div');
        deviceEl.className = 'device-item';
        deviceEl.innerHTML = `
            <div class="device-name">${device.name}</div>
            <div class="device-mac">${device.mac}</div>
            <div class="device-rssi">${device.rssi} dBm</div>
            <button class="btn-primary btn-small" onclick="connectBtDevice('${device.mac}')">Connect</button>
        `;
        deviceList.appendChild(deviceEl);
    });
}

// Connect to Bluetooth device
function connectBtDevice(mac) {
    appendLog(`[INFO] Connecting to Bluetooth device: ${mac}`, 'info');
    
    if (state.connected && state.writer) {
        const command = `$AERO,BT,CONNECT,${mac}\n`;
        state.writer.write(command);
    }
}

// Update telemetry mode
function updateTelemetryMode() {
    const mode = document.getElementById('telemetry-mode').value;
    
    appendLog(`[INFO] Setting telemetry mode: ${mode}`, 'info');
    
    if (state.connected && state.writer) {
        const command = `$AERO,TELEMETRY,MODE,${mode}\n`;
        state.writer.write(command);
    }
}

// Enhanced telemetry parser to include wireless data
function parseTelemetryLine(line) {
    // Check for wireless status messages
    if (line.startsWith('$AERO,WIFI,')) {
        const data = line.substring(12);
        processWirelessStatus('WIFI,' + data);
        return;
    }
    
    if (line.startsWith('$AERO,BT,')) {
        const data = line.substring(10);
        processWirelessStatus('BT,' + data);
        return;
    }
    
    if (line.startsWith('$AERO,TELEMETRY,')) {
        const data = line.substring(17);
        processWirelessStatus('TELEMETRY,' + data);
        return;
    }
    
    if (line.startsWith('$AERO,BT_DEVICES,')) {
        const data = line.substring(18);
        try {
            const devices = JSON.parse(data);
            processBtDevices(devices);
        } catch (e) {
            appendLog('[ERROR] Failed to parse BT devices list', 'error');
        }
        return;
    }
    
    // Original telemetry parsing
    const star = line.indexOf('*');
    if (star === -1) return;
    
    const csum = line.substring(star + 1);
    let computed = 0;
    for (let i = 1; i < star; i++) computed ^= line.charCodeAt(i);
    if (computed.toString(16).padStart(2, '0').toUpperCase() !== csum.toUpperCase()) return;

    const parts = line.substring(1, star).split(',');
    if (parts[0] !== 'AERO' || parts.length < 10) return;

    ingestTelemetry({
        tick: parseInt(parts[1]),
        roll: parseFloat(parts[2]),
        pitch: parseFloat(parts[3]),
        yaw: parseFloat(parts[4]),
        alt: parseFloat(parts[5]),
        lat: parseFloat(parts[6]),
        lon: parseFloat(parts[7]),
        sats: parseInt(parts[8]),
        co: parseFloat(parts[9]),
        /* Control fields (if present) */
        roll_sp: parts[10] ? parseFloat(parts[10]) : 0,
        pitch_sp: parts[11] ? parseFloat(parts[11]) : 0,
        roll_act: parts[12] ? parseFloat(parts[12]) : 0,
        pitch_act: parts[13] ? parseFloat(parts[13]) : 0,
    });
}

/* ============================================================
 * Telemetry ingest (from serial or demo)
 * ============================================================ */
function ingestTelemetry(d) {
    state.roll = d.roll;
    state.pitch = d.pitch;
    state.yaw = d.yaw;
    state.tick_ms = d.tick;

    /* Update Control UI */
    updateStabilizationUI(d);

    /* Update chart data */
    const ch = chartData.channels;
    ch.alt.push(d.alt); ch.alt.shift();
    ch.roll.push(d.roll); ch.roll.shift();
    ch.pitch.push(d.pitch); ch.pitch.shift();
    ch.yaw.push(d.yaw); ch.yaw.shift();
    ch.co.push(d.co); ch.co.shift();

    state.frameCount++;

    /* FPS calculation */
    const now = Date.now();
    if (now - state.lastRateTime >= 1000) {
        state.fps = state.frameCount;
        state.frameCount = 0;
        state.lastRateTime = now;
        document.getElementById('chart-fps').textContent = state.fps + ' Hz';
        document.getElementById('footer-rate').textContent = state.fps + ' fps';
        document.getElementById('footer-tick').textContent = state.tick_ms + ' ms';
        document.getElementById('footer-heap').textContent = (state.free_heap / 1024).toFixed(1) + ' KB';
    }
}

// Enhanced GPS data display with more detailed information
function updateGPSDisplay(gpsData) {
    // Update main GPS card
    setNum('lat', gpsData.lat.toFixed(6) + '°');
    setNum('lon', gpsData.lon.toFixed(6) + '°');
    setNum('gnss-alt', gpsData.alt.toFixed(1) + ' m');
    setNum('gnss-spd', gpsData.speed.toFixed(2) + ' m/s');
    setNum('gnss-sats', gpsData.sats.toString());
    setNum('gnss-hdop', gpsData.hdop ? gpsData.hdop.toFixed(2) : '—');
    setNum('gnss-course', gpsData.course ? gpsData.course.toFixed(1) + '°' : '—°');
    setNum('gnss-fix', getFixTypeText(gpsData.fix_type));
    
    // Update GPS status badge
    const gnssBadge = document.getElementById('badge-gnss');
    if (gpsData.sats >= 4 && gpsData.valid) {
        gnssBadge.className = 'sensor-badge healthy';
        gnssBadge.textContent = '3D FIX';
    } else if (gpsData.sats >= 3 && gpsData.valid) {
        gnssBadge.className = 'sensor-badge warn';
        gnssBadge.textContent = '2D FIX';
    } else {
        gnssBadge.className = 'sensor-badge warn';
        gnssBadge.textContent = 'NO FIX';
    }
    
    // Update map stats
    setNum('mstat-lat', gpsData.lat.toFixed(6));
    setNum('mstat-lon', gpsData.lon.toFixed(6));
    setNum('mstat-alt', gpsData.alt.toFixed(1) + ' m');
    setNum('mstat-sats', gpsData.sats.toString());
    
    // Update map
    updateMap(gpsData.lat, gpsData.lon, gpsData.alt, gpsData.sats);
    
    // Pulse GPS card to show update
    pulseCard('card-gnss');
}

// Helper function to get fix type text
function getFixTypeText(fixType) {
    switch (fixType) {
        case 0: return 'NONE';
        case 1: return '2D';
        case 2: return '3D';
        case 3: return 'RTK';
        default: return '—';
    }
}
function updateSensorDisplay(s) {
    /* IMU */
    setNum('ax', s.ax.toFixed(3)); setNum('ay', s.ay.toFixed(3)); setNum('az', s.az.toFixed(3));
    setNum('gx', s.gx.toFixed(4)); setNum('gy', s.gy.toFixed(4)); setNum('gz', s.gz.toFixed(4));
    pulseCard('card-imu');

    /* Barometer */
    const ch = chartData.channels;
    ch.pressure.push(s.pressure); ch.pressure.shift();
    ch.temp.push(s.temp_env); ch.temp.shift();
    setNum('baro-pressure', s.pressure.toFixed(1) + ' hPa');
    setNum('baro-alt', s.alt_baro.toFixed(1) + ' m');
    setNum('baro-temp', s.temp_baro.toFixed(1) + ' °C');
    pulseCard('card-baro');

    /* Magnetometer & Compass */
    setNum('mag-x', s.mx.toFixed(3)); setNum('mag-y', s.my.toFixed(3)); setNum('mag-z', s.mz.toFixed(3));
    const heading = Math.atan2(s.my, s.mx) * 180 / Math.PI;
    if (heading < 0) heading += 360;
    setNum('heading', heading.toFixed(1));
    updateCompass(heading);

    /* GNSS - Use enhanced GPS display */
    const gpsData = {
        lat: s.lat,
        lon: s.lon,
        alt: s.alt_gnss,
        speed: s.speed,
        sats: s.sats,
        hdop: s.hdop,
        course: s.course || 0,
        fix_type: s.sats >= 4 ? 2 : (s.sats >= 3 ? 1 : 0),
        valid: s.sats > 0
    };
    updateGPSDisplay(gpsData);

    /* CO */
    const ppm = s.co;
    updateCOGauge(ppm);
    const coBadge = document.getElementById('badge-co');
    if (ppm < 9) { coBadge.className = 'sensor-badge healthy'; coBadge.textContent = 'SAFE'; }
    else if (ppm < 35) { coBadge.className = 'sensor-badge warn'; coBadge.textContent = 'CAUTION'; }
    else { coBadge.className = 'sensor-badge error'; coBadge.textContent = 'DANGER'; }
    pulseCard('card-co');

    /* Environment */
    setNum('env-temp', s.temp_env.toFixed(1) + ' °C');
    setNum('env-hum', s.hum_env.toFixed(1) + ' %');
    pulseCard('card-env');
}

function setNum(id, val) {
    const el = document.getElementById(id);
    if (el) el.textContent = val;
}

function pulseCard(id) {
    const el = document.getElementById(id);
    if (!el) return;
    el.classList.remove('active-read');
    void el.offsetWidth;
    el.classList.add('active-read');
}

/* ============================================================
 * Compass
 * ============================================================ */
function updateCompass(heading_deg) {
    const needle = document.getElementById('compass-needle');
    if (needle) needle.setAttribute('transform', `rotate(${heading_deg}, 40, 40)`);
}

function updateCOGauge(ppm) {
    const max = 5000;
    const arc = document.getElementById('co-gauge-arc');
    const txt = document.getElementById('co-val-text');
    if (!arc || !txt) return;
    const pct = Math.min(ppm / max, 1);
    const total = 188.5;
    const offset = total * (1 - pct);
    arc.style.strokeDashoffset = offset;
    txt.textContent = ppm.toFixed(0);
}

/* ============================================================
 * Flight Control (PID) UI
 * ============================================================ */
function updateStabilizationUI(d) {
    if (!d.hasOwnProperty('roll_sp')) return;

    /* Roll Loop */
    setNum('pid-roll-sp', d.roll_sp.toFixed(1) + '°');
    setNum('pid-roll-val', d.roll.toFixed(1) + '°');
    const rollErr = d.roll_sp - d.roll;
    const rollErrEl = document.getElementById('pid-roll-err');
    if (rollErrEl) {
        rollErrEl.textContent = `Error: ${rollErr.toFixed(2)}°`;
        rollErrEl.className = 'loop-error ' + (Math.abs(rollErr) > 5 ? 'error-neg' : 'error-pos');
    }

    /* Bars: 0-100% where 50% is 0 degrees. Range +/- 45 deg */
    const r_sp_pct = 50 + (d.roll_sp / 45) * 50;
    const r_val_pct = 50 + (d.roll / 45) * 50;
    const barRollSp = document.getElementById('bar-roll-sp');
    const barRollVal = document.getElementById('bar-roll-val');
    if (barRollSp) barRollSp.style.width = Math.max(0, Math.min(100, r_sp_pct)) + '%';
    if (barRollVal) barRollVal.style.width = Math.max(0, Math.min(100, r_val_pct)) + '%';

    /* Pitch Loop */
    setNum('pid-pitch-sp', d.pitch_sp.toFixed(1) + '°');
    setNum('pid-pitch-val', d.pitch.toFixed(1) + '°');
    const pitchErr = d.pitch_sp - d.pitch;
    const pitchErrEl = document.getElementById('pid-pitch-err');
    if (pitchErrEl) {
        pitchErrEl.textContent = `Error: ${pitchErr.toFixed(2)}°`;
        pitchErrEl.className = 'loop-error ' + (Math.abs(pitchErr) > 5 ? 'error-neg' : 'error-pos');
    }

    const p_sp_pct = 50 + (d.pitch_sp / 45) * 50;
    const p_val_pct = 50 + (d.pitch / 45) * 50;
    const barPitchSp = document.getElementById('bar-pitch-sp');
    const barPitchVal = document.getElementById('bar-pitch-val');
    if (barPitchSp) barPitchSp.style.width = Math.max(0, Math.min(100, p_sp_pct)) + '%';
    if (barPitchVal) barPitchVal.style.width = Math.max(0, Math.min(100, p_val_pct)) + '%';

    /* Actuator Outputs (0 to 1 -> 0 to 100%) */
    const outRoll = document.getElementById('out-roll');
    const outPitch = document.getElementById('out-pitch');
    const outYaw = document.getElementById('out-yaw');
    const outThr = document.getElementById('out-thr');

    if (outRoll) outRoll.style.height = (Math.abs(d.roll_act || 0) * 100).toFixed(0) + '%';
    if (outPitch) outPitch.style.height = (Math.abs(d.pitch_act || 0) * 100).toFixed(0) + '%';
    if (outYaw) outYaw.style.height = (Math.abs(d.yaw_act || 0) * 100).toFixed(0) + '%';
    if (outThr) outThr.style.height = ((d.thr_act || 0.1) * 100).toFixed(0) + '%';
}

/* ============================================================
 * Leaflet Map Logic
 * ============================================================ */
function initMap() {
    state.map = L.map('map').setView([12.9716, 77.5946], 13);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap contributors'
    }).addTo(state.map);

    state.marker = L.marker([12.9716, 77.5946]).addTo(state.map);
    state.path = L.polyline([], { color: '#00d4ff', weight: 3 }).addTo(state.map);
}

function updateMap(lat, lon, alt, sats) {
    if (!state.map) return;
    if (isNaN(lat) || isNaN(lon) || lat === 0) return;

    const pos = [lat, lon];
    state.marker.setLatLng(pos);

    state.pathCoords.push(pos);
    if (state.pathCoords.length > 500) state.pathCoords.shift();
    state.path.setLatLngs(state.pathCoords);

    state.map.panTo(pos);

    /* Update mini stats */
    setNum('mstat-lat', lat.toFixed(6));
    setNum('mstat-lon', lon.toFixed(6));
    setNum('mstat-alt', alt.toFixed(1) + ' m');
    setNum('mstat-sats', sats);
}

/* ============================================================
 * EKF State update
 * ============================================================ */
function updateEKFState(ekf) {
    state.pos = ekf.pos;
    state.vel = ekf.vel;
    state.roll = ekf.roll;
    state.pitch = ekf.pitch;
    state.yaw = ekf.yaw;
    state.P_diag = ekf.P_diag;
    state.updates = ekf.updates;
    state.innov = ekf.innov;
    state.free_heap = ekf.free_heap;

    /* Attitude tab */
    if (state.tab === 'attitude') {
        const r = ekf.roll, p = ekf.pitch, y = ekf.yaw;
        document.getElementById('roll-disp').textContent = r.toFixed(1) + '°';
        document.getElementById('pitch-disp').textContent = p.toFixed(1) + '°';
        document.getElementById('yaw-disp').textContent = y.toFixed(1) + '°';

        document.getElementById('roll-num').textContent = r.toFixed(2) + '°';
        document.getElementById('pitch-num').textContent = p.toFixed(2) + '°';
        document.getElementById('yaw-num').textContent = y.toFixed(2) + '°';

        /* Euler bars — 50% = zero, ±180 maps to ±50% */
        const rf = 50 + (r / 180) * 45;
        const pf = 50 + (p / 90) * 45;
        const yf = (y / 360) * 100;
        setEulerBar('roll-bar', rf);
        setEulerBar('pitch-bar', pf);
        document.getElementById('yaw-bar').style.width = yf + '%';
        document.getElementById('yaw-bar').style.left = '0%';

        /* Nav state */
        setNum('pos-n', ekf.pos[0].toFixed(2) + ' m');
        setNum('pos-e', ekf.pos[1].toFixed(2) + ' m');
        setNum('pos-d', (-ekf.pos[2]).toFixed(2) + ' m');
        setNum('vel-n', ekf.vel[0].toFixed(2) + ' m/s');
        setNum('vel-e', ekf.vel[1].toFixed(2) + ' m/s');
        setNum('vel-d', ekf.vel[2].toFixed(2) + ' m/s');
        const healthEl = document.getElementById('ekf-health');
        const healthy = ekf.innov.NIS_baro < 6 && ekf.innov.NIS_gps < 18;
        healthEl.textContent = healthy ? 'OK' : 'DEGRADED';
        healthEl.className = 'nav-val ' + (healthy ? 'health-ok' : 'health-bad');
    }

    /* EKF tab */
    if (state.tab === 'ekf') renderEKFTable(ekf);
}

function setEulerBar(id, pct) {
    const el = document.getElementById(id);
    if (!el) return;
    const center = 50;
    const diff = pct - center;
    if (diff >= 0) { el.style.left = center + '%'; el.style.width = diff + '%'; }
    else { el.style.left = pct + '%'; el.style.width = (-diff) + '%'; }
}

/* ============================================================
 * EKF State Table
 * ============================================================ */
const EKF_STATES = [
    [0, 'pos[0]', 'p_North', 'm'],
    [1, 'pos[1]', 'p_East', 'm'],
    [2, 'pos[2]', 'p_Down', 'm'],
    [3, 'vel[0]', 'v_North', 'm/s'],
    [4, 'vel[1]', 'v_East', 'm/s'],
    [5, 'vel[2]', 'v_Down', 'm/s'],
    [6, 'quat[w]', 'q_w', '—'],
    [7, 'quat[x]', 'q_x', '—'],
    [8, 'quat[y]', 'q_y', '—'],
    [9, 'quat[z]', 'q_z', '—'],
    [10, 'ba[x]', 'accel_bias_X', 'm/s²'],
    [11, 'ba[y]', 'accel_bias_Y', 'm/s²'],
    [12, 'ba[z]', 'accel_bias_Z', 'm/s²'],
    [13, 'bg[x]', 'gyro_bias_X', 'rad/s'],
    [14, 'bg[y]', 'gyro_bias_Y', 'rad/s'],
];

function renderEKFTable(ekf) {
    const allVals = [...ekf.pos, ...ekf.vel, ...ekf.quat,
    ...ekf.accel_bias, ...ekf.gyro_bias];
    const tbody = document.getElementById('ekf-table-body');
    if (!tbody) return;
    tbody.innerHTML = EKF_STATES.map(([i, sym, name, unit]) => {
        const v = allVals[i] ?? 0;
        const s = Math.sqrt(Math.abs(ekf.P_diag[i] ?? 0));
        return `<tr>
      <td>${i}</td>
      <td>${name}</td>
      <td style="font-family:var(--font-mono);color:var(--text-dim)">${sym}</td>
      <td>${v.toFixed(6)} <span style="color:var(--text-dim);font-size:.7em">${unit}</span></td>
      <td>±${s.toFixed(6)}</td>
    </tr>`;
    }).join('');

    /* Innovation display */
    setNum('innov-baro', (ekf.innov.baro ?? 0).toFixed(3) + ' m');
    setNum('innov-gps-n', (ekf.innov.gpsN ?? 0).toFixed(3) + ' m');
    setNum('innov-gps-e', (ekf.innov.gpsE ?? 0).toFixed(3) + ' m');
    setNum('nis-baro', (ekf.innov.NIS_baro ?? 0).toFixed(3));
    setNum('nis-gps', (ekf.innov.NIS_gps ?? 0).toFixed(3));
    setNum('innov-mag-x', (ekf.innov.magX ?? 0).toFixed(5) + ' G');
    setNum('ekf-updates', ekf.updates ?? 0);
}

/* ============================================================
 * Artificial Horizon Indicator (AHI)
 * ============================================================ */
const ahiCanvas = document.getElementById('ahi-canvas');
const ahiCtx = ahiCanvas ? ahiCanvas.getContext('2d') : null;

function drawAHI(roll_deg, pitch_deg) {
    if (!ahiCtx) return;
    const W = ahiCanvas.width, H = ahiCanvas.height;
    const cx = W / 2, cy = H / 2, R = W / 2 - 4;
    const roll = roll_deg * Math.PI / 180;
    const pitch = pitch_deg;

    ahiCtx.clearRect(0, 0, W, H);

    /* Clip to circle */
    ahiCtx.save();
    ahiCtx.beginPath();
    ahiCtx.arc(cx, cy, R, 0, Math.PI * 2);
    ahiCtx.clip();

    /* Sky */
    const skyGrad = ahiCtx.createLinearGradient(0, 0, 0, H);
    skyGrad.addColorStop(0, '#0a2a5e');
    skyGrad.addColorStop(1, '#1a5fb4');
    ahiCtx.fillStyle = skyGrad;
    ahiCtx.fillRect(0, 0, W, H);

    /* Ground & horizon — rotate by roll, shift by pitch */
    ahiCtx.save();
    ahiCtx.translate(cx, cy);
    ahiCtx.rotate(roll);
    const pitchOffset = pitch * 3; /* pixels per degree */

    /* Ground */
    const gndGrad = ahiCtx.createLinearGradient(0, pitchOffset, 0, R + pitchOffset);
    gndGrad.addColorStop(0, '#5c3a1e');
    gndGrad.addColorStop(1, '#2d1f0e');
    ahiCtx.fillStyle = gndGrad;
    ahiCtx.fillRect(-W, pitchOffset, W * 2, H);

    /* Horizon line */
    ahiCtx.strokeStyle = 'white';
    ahiCtx.lineWidth = 2;
    ahiCtx.beginPath();
    ahiCtx.moveTo(-W, pitchOffset);
    ahiCtx.lineTo(W, pitchOffset);
    ahiCtx.stroke();

    /* Pitch ladder */
    ahiCtx.fillStyle = 'white';
    ahiCtx.font = 'bold 11px JetBrains Mono, monospace';
    for (let deg = -30; deg <= 30; deg += 10) {
        if (deg === 0) continue;
        const y = pitchOffset - deg * 3;
        const len = deg % 10 === 0 ? 40 : 20;
        ahiCtx.strokeStyle = 'rgba(255,255,255,0.7)';
        ahiCtx.lineWidth = 1.5;
        ahiCtx.beginPath();
        ahiCtx.moveTo(-len, y);
        ahiCtx.lineTo(len, y);
        ahiCtx.stroke();
        ahiCtx.fillText(Math.abs(deg), len + 4, y + 4);
    }

    ahiCtx.restore(); /* undo roll */

    /* Bank scale arc */
    ahiCtx.strokeStyle = 'rgba(255,255,255,0.4)';
    ahiCtx.lineWidth = 1;
    for (const a of [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]) {
        const rad = (a - 90) * Math.PI / 180;
        ahiCtx.beginPath();
        ahiCtx.moveTo(cx + (R - 14) * Math.cos(rad), cy + (R - 14) * Math.sin(rad));
        ahiCtx.lineTo(cx + (R - 4) * Math.cos(rad), cy + (R - 4) * Math.sin(rad));
        ahiCtx.stroke();
    }

    /* Roll pointer */
    ahiCtx.save();
    ahiCtx.translate(cx, cy);
    ahiCtx.rotate(roll);
    ahiCtx.fillStyle = '#00d4ff';
    ahiCtx.beginPath();
    ahiCtx.moveTo(0, -(R - 6));
    ahiCtx.lineTo(-6, -(R - 14));
    ahiCtx.lineTo(6, -(R - 14));
    ahiCtx.closePath();
    ahiCtx.fill();
    ahiCtx.restore();

    ahiCtx.restore(); /* undo clip */

    /* Circle border */
    ahiCtx.beginPath();
    ahiCtx.arc(cx, cy, R, 0, Math.PI * 2);
    ahiCtx.strokeStyle = 'rgba(0,212,255,0.4)';
    ahiCtx.lineWidth = 2;
    ahiCtx.stroke();

    /* Fixed aircraft symbol */
    ahiCtx.strokeStyle = '#ffd600';
    ahiCtx.lineWidth = 3;
    ahiCtx.lineCap = 'round';
    /* Left wing */
    ahiCtx.beginPath();
    ahiCtx.moveTo(cx - 60, cy);
    ahiCtx.lineTo(cx - 20, cy);
    ahiCtx.stroke();
    /* Right wing */
    ahiCtx.beginPath();
    ahiCtx.moveTo(cx + 20, cy);
    ahiCtx.lineTo(cx + 60, cy);
    ahiCtx.stroke();
    /* Fuselage dot */
    ahiCtx.beginPath();
    ahiCtx.arc(cx, cy, 4, 0, Math.PI * 2);
    ahiCtx.fillStyle = '#ffd600';
    ahiCtx.fill();
}

/* ============================================================
 * Time-Series Chart
 * ============================================================ */
const chartCanvas = document.getElementById('chart-canvas');
const chartCtx = chartCanvas ? chartCanvas.getContext('2d') : null;

function drawChart() {
    if (!chartCtx) return;
    const channel = document.getElementById('chart-channel')?.value || 'alt';
    const data = chartData.channels[channel];
    const W = chartCanvas.offsetWidth, H = 140;
    chartCanvas.width = W;
    chartCanvas.height = H;

    chartCtx.clearRect(0, 0, W, H);

    /* Background */
    chartCtx.fillStyle = '#060d1a';
    chartCtx.fillRect(0, 0, W, H);

    if (!data || data.length < 2) return;

    const min_v = Math.min(...data), max_v = Math.max(...data);
    const range = max_v - min_v || 1;
    const padX = 50, padY = 12;
    const plotW = W - padX - 8, plotH = H - padY * 2;

    /* Grid */
    chartCtx.strokeStyle = 'rgba(26,45,74,0.8)';
    chartCtx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
        const y = padY + (plotH / 4) * i;
        chartCtx.beginPath(); chartCtx.moveTo(padX, y); chartCtx.lineTo(W - 8, y); chartCtx.stroke();
        const val = max_v - (range / 4) * i;
        chartCtx.fillStyle = '#475569';
        chartCtx.font = '9px JetBrains Mono, monospace';
        chartCtx.fillText(val.toFixed(1), 2, y + 3);
    }

    /* Gradient fill */
    const grad = chartCtx.createLinearGradient(0, padY, 0, padY + plotH);
    grad.addColorStop(0, 'rgba(0,212,255,0.25)');
    grad.addColorStop(1, 'rgba(0,212,255,0)');

    chartCtx.beginPath();
    data.forEach((v, i) => {
        const x = padX + (i / (data.length - 1)) * plotW;
        const y = padY + plotH - ((v - min_v) / range) * plotH;
        i === 0 ? chartCtx.moveTo(x, y) : chartCtx.lineTo(x, y);
    });
    const lastX = padX + plotW;
    const lastY = padY + plotH;
    chartCtx.lineTo(lastX, lastY); chartCtx.lineTo(padX, lastY);
    chartCtx.closePath();
    chartCtx.fillStyle = grad;
    chartCtx.fill();

    /* Line */
    chartCtx.beginPath();
    data.forEach((v, i) => {
        const x = padX + (i / (data.length - 1)) * plotW;
        const y = padY + plotH - ((v - min_v) / range) * plotH;
        i === 0 ? chartCtx.moveTo(x, y) : chartCtx.lineTo(x, y);
    });
    chartCtx.strokeStyle = '#00d4ff';
    chartCtx.lineWidth = 1.5;
    chartCtx.stroke();

    /* Current value dot */
    const lastVal = data[data.length - 1];
    const dotX = padX + plotW;
    const dotY = padY + plotH - ((lastVal - min_v) / range) * plotH;
    chartCtx.beginPath();
    chartCtx.arc(dotX, dotY, 3, 0, Math.PI * 2);
    chartCtx.fillStyle = '#00d4ff';
    chartCtx.fill();
    chartCtx.fillStyle = '#00d4ff';
    chartCtx.font = 'bold 10px JetBrains Mono';
    chartCtx.fillText(lastVal.toFixed(2), dotX - 28, dotY - 6);
}

function updateChart() { drawChart(); }

/* ============================================================
 * Flash workflow
 * ============================================================ */
function handleDrop(e) {
    e.preventDefault();
    const f = e.dataTransfer.files[0];
    if (f) setFlashFile(f);
}

function handleFileSelect(e) {
    const f = e.target.files[0];
    if (f) setFlashFile(f);
}

function setFlashFile(f) {
    state.selectedFile = f;
    document.getElementById('file-name').textContent = f.name;
    document.getElementById('file-size').textContent = (f.size / 1024).toFixed(1) + ' KB';
    document.getElementById('file-info').style.display = 'flex';
    document.getElementById('dropzone').style.borderColor = 'var(--accent-cyan)';
    document.getElementById('btn-flash-action').disabled = !state.connected;
    appendLog(`[INFO] Selected firmware: ${f.name} (${(f.size / 1024).toFixed(1)} KB)`, 'info');
}

async function startFlash() {
    if (!state.selectedFile) {
        appendLog('[ERR] No firmware file selected', 'err');
        return;
    }

    const mcu = document.getElementById('mcu-select').value;
    const erase = document.getElementById('opt-erase').checked;
    const verify = document.getElementById('opt-verify').checked;
    const reset = document.getElementById('opt-reset').checked;

    /* Update UI for flash start */
    document.getElementById('btn-flash-action').disabled = true;
    const progressContainer = document.getElementById('progress-container');
    const progressTarget = document.getElementById('progress-target-display');
    const progressFill = document.getElementById('progress-fill');
    const progressLabel = document.getElementById('progress-label');
    const progressPct = document.getElementById('progress-pct');

    progressContainer.style.display = 'block';
    progressTarget.textContent = `Target: ${mcu}`;
    progressFill.style.width = '0%';
    progressPct.textContent = '0%';
    progressLabel.textContent = 'Preparing...';

    appendLog(`\n[INFO] Starting real flash session for ${mcu}...`, 'info');
    appendLog(`[INFO] Uploading: ${state.selectedFile.name} (${(state.selectedFile.size / 1024).toFixed(1)} KB)`, 'info');

    /* Create form data for upload */
    const formData = new FormData();
    formData.append('file', state.selectedFile);
    formData.append('mcu', mcu);
    formData.append('port', document.getElementById('port-select').value);
    formData.append('erase', erase);
    formData.append('verify', verify);
    formData.append('reset', reset);

    progressLabel.textContent = 'Uploading & Flashing...';
    progressFill.style.width = '30%';
    progressPct.textContent = '30%';

    try {
        const response = await fetch('http://localhost:8001/api/flash', {
            method: 'POST',
            body: formData
        });

        const result = await response.json();

        if (result.log) {
            result.log.forEach(line => {
                const type = line.includes('[ERR]') || line.includes('error') ? 'err' :
                    line.includes('[WARN]') ? 'warn' : 'info';
                appendLog(line, type);
            });
        }

        if (result.success) {
            progressFill.style.width = '100%';
            progressPct.textContent = '100%';
            progressLabel.textContent = 'Complete';
            appendLog('\n[ ✓ ] FLASH COMPLETE — Firmware deployed successfully!', 'info');
            if (result.simulated) {
                appendLog('[NOTE] This was a SIMULATED flash as no programmer was found on server.', 'warn');
            }
        } else {
            progressFill.style.background = 'var(--accent-red)';
            progressLabel.textContent = 'Flash Failed';
            appendLog('\n[ ✗ ] FLASH FAILED — Check the log for details.', 'err');
        }

    } catch (err) {
        appendLog(`[ERR] API call failed: ${err.message}`, 'err');
        progressLabel.textContent = 'Network Error';
    } finally {
        document.getElementById('btn-flash-action').disabled = false;
    }
}

async function recalibrateSensors() {
    if (!state.connected) return;

    appendLog('[CMD] Sending Recalibration command ($CMD,CAL)...', 'info');
    const btn = document.getElementById('btn-recal');
    btn.disabled = true;
    btn.textContent = 'Calibrating...';

    try {
        const response = await fetch('http://localhost:8001/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ cmd: '$CMD,CAL' })
        });
        const result = await response.json();
        if (result.success) {
            appendLog('[OK] Command acknowledged by server.', 'info');
            /* Firmware provides feedback via log, so we just wait */
            setTimeout(() => {
                btn.disabled = false;
                btn.textContent = 'Recalibrate Sensors';
            }, 3000);
        } else {
            appendLog(`[ERR] Command failed: ${result.detail}`, 'err');
            btn.disabled = false;
            btn.textContent = 'Recalibrate Sensors';
        }
    } catch (err) {
        appendLog(`[ERR] Network error: ${err.message}`, 'err');
        btn.disabled = false;
        btn.textContent = 'Recalibrate Sensors';
    }
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function refreshPorts() {
    const sel = document.getElementById('port-select');
    if (!sel) return;

    appendLog('[INFO] Scanning for serial ports (local server)...', 'info');

    try {
        const response = await fetch('http://localhost:8001/api/ports');
        const data = await response.json();
        const ports = data.ports || [];

        sel.innerHTML = '<option value="">— Select Port —</option>';
        if (ports.length === 0) {
            appendLog('[WARN] No serial ports found. Try reconnecting your hardware.', 'warn');
        } else {
            ports.forEach(p => {
                const opt = document.createElement('option');
                opt.value = p.port;
                opt.textContent = `${p.port} (${p.desc || 'USB Serial Device'})`;
                sel.appendChild(opt);
            });
            appendLog(`[OK] Detected ${ports.length} port(s). Ready to connect.`, 'info');
        }
    } catch (err) {
        appendLog(`[ERR] Server communication error: ${err.message}`, 'err');
        /* Fallback for local demo simulation */
        sel.innerHTML = '<option value="">— Select Port —</option>';
        sel.innerHTML += '<option value="DEMO">DEMO (simulated)</option>';
        sel.innerHTML += '<option value="COM3">COM3 (ST-LINK)</option>';
        sel.innerHTML += '<option value="COM10">COM10 (ESP32 USB)</option>';
    }
}

/* ============================================================
 * Log helpers
 * ============================================================ */
function appendLog(msg, type = 'text') {
    const log = document.getElementById('flash-log');
    if (!log) return;
    const prefix = new Date().toLocaleTimeString('en', { hour12: false });
    const line = `[${prefix}] ${msg}\n`;
    log.textContent += line;
    log.scrollTop = log.scrollHeight;
}

function clearLog() {
    const log = document.getElementById('flash-log');
    if (log) log.textContent = 'Log cleared.\n';
}

/* ============================================================
 * Main render loop
 * ============================================================ */
function renderLoop() {
    if (state.tab === 'attitude') drawAHI(state.roll, state.pitch);
    if (state.tab === 'monitor') drawChart();
    requestAnimationFrame(renderLoop);
}

/* ============================================================
 * Init
 * ============================================================ */
document.addEventListener('DOMContentLoaded', () => {
    /* Populate port selector */
    refreshPorts();

    /* Show initial MCU info */
    showDeviceInfo();

    /* Initialize wireless status icons */
    initializeWirelessIcons();

    /* Dragover style */
    const dz = document.getElementById('dropzone');
    if (dz) {
        dz.addEventListener('dragover', () => dz.classList.add('dragover'));
        dz.addEventListener('dragleave', () => dz.classList.remove('dragover'));
        dz.addEventListener('drop', () => dz.classList.remove('dragover'));
    }

    /* Start auto-connect in demo if serial not available */
    if (!('serial' in navigator)) {
        appendLog('[INFO] Web Serial API unavailable — click Connect to start demo.', 'warn');
    }

    /* Initialize Map */
    initMap();

    /* Start render loop */
    renderLoop();

    appendLog('[INFO] AeroFirmware Ground Station v1.0.0 initialized.', 'info');
    appendLog('[INFO] Supported MCUs: STM32H7, F4, F7, L0, L4, ESP32, ESP8266', 'info');
});

/* Initialize wireless status icons to off state */
function initializeWirelessIcons() {
    const wifiIcon = document.getElementById('wifi-status-icon');
    const btIcon = document.getElementById('bt-status-icon');
    
    if (wifiIcon) {
        wifiIcon.className = 'wireless-icon wifi-icon off';
        wifiIcon.setAttribute('title', 'WiFi: Off');
    }
    
    if (btIcon) {
        btIcon.className = 'wireless-icon bt-icon off';
        btIcon.setAttribute('title', 'Bluetooth: Off');
    }
}

/* ============================================================
 * Wireless Communication Functions
 * ============================================================ */

// WiFi Functions
function updateWifiMode() {
    const mode = document.getElementById('wifi-mode').value;
    const stationConfig = document.getElementById('wifi-station-config');
    const apConfig = document.getElementById('wifi-ap-config');
    const wifiInfo = document.getElementById('wifi-info');
    
    stationConfig.style.display = 'none';
    apConfig.style.display = 'none';
    wifiInfo.style.display = 'none';
    
    if (mode === 'station') {
        stationConfig.style.display = 'block';
    } else if (mode === 'ap') {
        apConfig.style.display = 'block';
    }
    
    updateWifiStatus(mode);
}

function connectWifi() {
    const ssid = document.getElementById('wifi-ssid').value;
    const password = document.getElementById('wifi-password').value;
    
    if (!ssid) {
        appendLog('[ERROR] WiFi SSID is required', 'error');
        return;
    }
    
    appendLog(`[INFO] Connecting to WiFi: ${ssid}`, 'info');
    
    // Send WiFi connect command to device
    if (state.connected && state.writer) {
        const command = `$AERO,WIFI,CONNECT,${ssid},${password}\n`;
        state.writer.write(command);
    }
    
    updateWifiStatus('connecting');
}

function startWifiAP() {
    const ssid = document.getElementById('ap-ssid').value;
    const channel = document.getElementById('ap-channel').value;
    
    if (!ssid) {
        appendLog('[ERROR] AP SSID is required', 'error');
        return;
    }
    
    appendLog(`[INFO] Starting WiFi AP: ${ssid} on channel ${channel}`, 'info');
    
    // Send WiFi AP command to device
    if (state.connected && state.writer) {
        const command = `$AERO,WIFI,AP,${ssid},,${channel}\n`;
        state.writer.write(command);
    }
    
    updateWifiStatus('ap');
}

function updateWifiStatus(status) {
    const dot = document.getElementById('wifi-status-dot');
    const label = document.getElementById('wifi-status-label');
    const info = document.getElementById('wifi-info');
    
    dot.className = 'status-dot';
    
    switch (status) {
        case 'connecting':
            dot.classList.add('connecting');
            label.textContent = 'Connecting...';
            break;
        case 'connected':
            dot.classList.add('connected');
            label.textContent = 'Connected';
            info.style.display = 'flex';
            break;
        case 'ap':
            dot.classList.add('connected');
            label.textContent = 'Access Point';
            info.style.display = 'flex';
            break;
        case 'off':
        default:
            label.textContent = 'Disconnected';
            info.style.display = 'none';
            break;
    }
}

// Bluetooth Functions
function updateBtMode() {
    const mode = document.getElementById('bt-mode').value;
    const discoverableConfig = document.getElementById('bt-discoverable-config');
    const btInfo = document.getElementById('bt-info');
    
    discoverableConfig.style.display = 'none';
    btInfo.style.display = 'none';
    
    if (mode === 'discoverable') {
        discoverableConfig.style.display = 'block';
    }
    
    updateBtStatus(mode);
}

function startBtDiscovery() {
    appendLog('[INFO] Starting Bluetooth device discovery', 'info');
    
    // Send BT discovery command to device
    if (state.connected && state.writer) {
        const command = `$AERO,BT,SCAN\n`;
        state.writer.write(command);
    }
    
    // Clear previous device list
    const deviceList = document.getElementById('bt-devices');
    deviceList.innerHTML = '';
}

function connectBtDevice(addr, name) {
    appendLog(`[INFO] Connecting to Bluetooth device: ${name}`, 'info');
    
    // Send BT connect command to device
    if (state.connected && state.writer) {
        const command = `$AERO,BT,CONNECT,${addr}\n`;
        state.writer.write(command);
    }
    
    updateBtStatus('connecting');
}

function updateBtStatus(status) {
    const dot = document.getElementById('bt-status-dot');
    const label = document.getElementById('bt-status-label');
    const info = document.getElementById('bt-info');
    
    dot.className = 'status-dot';
    
    switch (status) {
        case 'connecting':
            dot.classList.add('connecting');
            label.textContent = 'Connecting...';
            break;
        case 'connected':
            dot.classList.add('connected');
            label.textContent = 'Connected';
            info.style.display = 'flex';
            break;
        case 'discoverable':
            dot.classList.add('connected');
            label.textContent = 'Discoverable';
            break;
        case 'off':
        default:
            label.textContent = 'Disconnected';
            info.style.display = 'none';
            break;
    }
}

// Telemetry Functions
function updateTelemetryMode() {
    const mode = document.getElementById('telemetry-mode').value;
    
    appendLog(`[INFO] Setting telemetry mode: ${mode}`, 'info');
    
    // Send telemetry mode command to device
    if (state.connected && state.writer) {
        const command = `$AERO,TELEMETRY,MODE,${mode}\n`;
        state.writer.write(command);
    }
}

function updateTelemetryStats(rate, bytesPerSec) {
    const rateElement = document.getElementById('telemetry-rate');
    const bytesElement = document.getElementById('telemetry-bytes');
    
    if (rateElement) rateElement.textContent = `${rate} Hz`;
    if (bytesElement) bytesElement.textContent = `${(bytesPerSec / 1024).toFixed(1)} KB/s`;
}

// Enhanced GPS Data Parser for NEO-F10
function parseGPSData(data) {
    const parts = data.split(',');
    
    if (parts[0] === '$AERO' && parts[1] === 'GPS') {
        const lat = parseFloat(parts[2]) || 0;
        const lon = parseFloat(parts[3]) || 0;
        const alt = parseFloat(parts[4]) || 0;
        const speed = parseFloat(parts[5]) || 0;
        const course = parseFloat(parts[6]) || 0;
        const sats = parseInt(parts[7]) || 0;
        const fix = parseInt(parts[8]) || 0;
        const hdop = parseFloat(parts[9]) || 0;
        const vdop = parseFloat(parts[10]) || 0;
        
        // Update GPS display
        updateGPSDisplay(lat, lon, alt, speed, course, sats, fix, hdop, vdop);
        
        // Update map if valid coordinates
        if (lat !== 0 && lon !== 0) {
            updateMap(lat, lon, alt);
        }
    }
}

function updateGPSDisplay(lat, lon, alt, speed, course, sats, fix, hdop, vdop) {
    // Update GPS card values
    document.getElementById('lat').textContent = lat.toFixed(6);
    document.getElementById('lon').textContent = lon.toFixed(6);
    document.getElementById('gnss-alt').textContent = `${alt.toFixed(1)} m`;
    document.getElementById('gnss-spd').textContent = `${speed.toFixed(2)} m/s`;
    document.getElementById('gnss-sats').textContent = sats;
    document.getElementById('gnss-hdop').textContent = hdop.toFixed(1);
    document.getElementById('gnss-course').textContent = `${course.toFixed(1)}°`;
    
    // Update fix type and badge
    const badge = document.getElementById('badge-gnss');
    const fixTypeElement = document.getElementById('gnss-fix');
    
    let fixText = 'NO FIX';
    let badgeClass = '';
    
    switch (fix) {
        case 1:
            fixText = '2D';
            badgeClass = 'warning';
            break;
        case 2:
            fixText = '3D';
            badgeClass = 'healthy';
            break;
        case 3:
            fixText = 'RTCM';
            badgeClass = 'healthy';
            break;
        default:
            badgeClass = '';
            break;
    }
    
    fixTypeElement.textContent = fixText;
    badge.className = `sensor-badge ${badgeClass}`;
    
    // Update map stats
    document.getElementById('mstat-lat').textContent = lat.toFixed(6);
    document.getElementById('mstat-lon').textContent = lon.toFixed(6);
    document.getElementById('mstat-alt').textContent = `${alt.toFixed(1)} m`;
    document.getElementById('mstat-sats').textContent = sats;
}

// Enhanced frame parser to handle wireless and GPS data
function parseFrame(line) {
    // Original frame parsing logic...
    if (line.startsWith('$AERO,')) {
        const fields = line.slice(6).split(',');
        const type = fields[0];
        
        switch (type) {
            case 'GPS':
                parseGPSData(line);
                break;
            case 'WIFI':
                parseWifiData(fields);
                break;
            case 'BT':
                parseBtData(fields);
                break;
            case 'TELEMETRY':
                parseTelemetryData(fields);
                break;
            // ... other existing cases
        }
    }
}

function parseWifiData(fields) {
    if (fields[1] === 'STATUS') {
        const status = fields[2];
        const ip = fields[3] || '—';
        const rssi = fields[4] || '—';
        
        updateWifiStatus(status);
        
        if (ip !== '—') {
            document.getElementById('wifi-ip').textContent = ip;
        }
        if (rssi !== '—') {
            document.getElementById('wifi-rssi').textContent = `${rssi} dBm`;
        }
    }
}

function parseBtData(fields) {
    if (fields[1] === 'DEVICE') {
        const addr = fields[2];
        const name = fields[3];
        const rssi = fields[4];
        
        addBtDeviceToList(addr, name, rssi);
    } else if (fields[1] === 'STATUS') {
        const status = fields[2];
        const device = fields[3] || '—';
        const rssi = fields[4] || '—';
        
        updateBtStatus(status);
        
        if (device !== '—') {
            document.getElementById('bt-device').textContent = device;
        }
        if (rssi !== '—') {
            document.getElementById('bt-rssi').textContent = `${rssi} dBm`;
        }
    }
}

function parseTelemetryData(fields) {
    if (fields[1] === 'STATS') {
        const rate = parseFloat(fields[2]) || 0;
        const bytes = parseFloat(fields[3]) || 0;
        
        updateTelemetryStats(rate, bytes);
    }
}

function addBtDeviceToList(addr, name, rssi) {
    const deviceList = document.getElementById('bt-devices');
    
    // Check if device already exists
    const existingDevice = document.querySelector(`[data-addr="${addr}"]`);
    if (existingDevice) return;
    
    const deviceItem = document.createElement('div');
    deviceItem.className = 'device-item';
    deviceItem.setAttribute('data-addr', addr);
    deviceItem.onclick = () => connectBtDevice(addr, name);
    
    deviceItem.innerHTML = `
        <span class="device-name">${name}</span>
        <span class="device-rssi">${rssi} dBm</span>
    `;
    
    deviceList.appendChild(deviceItem);
}
