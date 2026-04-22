'use strict';

let ws = null;

let ntpOffset = 0;   // seconds
let ntpSynced = false;
let syncCount = 0;
let syncTimer = null;

const EMA_ALPHA = 0.1;

// ── Logging ────────────────────────────────────────────────
function log(msg, type = 'info') {
    const box = document.getElementById('log');
    const line = document.createElement('span');
    line.className = type;
    line.textContent = msg + '\n';
    box.appendChild(line);
    box.scrollTop = box.scrollHeight;
}

// ── UI helpers ─────────────────────────────────────────────
function setDot(id, color) {
    document.getElementById(id).className = 'dot ' + color;
}

function setVal(id, text) {
    document.getElementById(id).textContent = text;
}

// ── High precision NTP time (seconds) ──────────────────────
function nowNtpSeconds() {
    const NTP_UNIX_DELTA = 2208988800.0;

    // Combine Date + performance for better precision
    const unixMs = Date.now() + performance.now() % 1;
    return (unixMs / 1000.0) + NTP_UNIX_DELTA;
}

// ── Send sync request ──────────────────────────────────────
function sendNtpSync() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    const t1 = nowNtpSeconds();
    ws.send(JSON.stringify({ type: 'ntp_sync', t1 }));
}

// ── Handle response ────────────────────────────────────────
function handleNtpResponse(msg) {
    const T1 = msg.t1;
    const T2 = msg.t2;
    const T3 = msg.t3;
    const T4 = nowNtpSeconds();

    const rawOffset = ((T2 - T1) + (T3 - T4)) / 2.0;

    if (!ntpSynced) {
        ntpOffset = rawOffset;
        ntpSynced = true;
    } else {
        ntpOffset = EMA_ALPHA * rawOffset + (1 - EMA_ALPHA) * ntpOffset;
    }

    syncCount++;

    const offsetMs = (ntpOffset * 1000).toFixed(3);
    const sign = ntpOffset >= 0 ? '+' : '';

    const display = document.getElementById('offsetDisplay');
    display.textContent = sign + offsetMs;
    display.className = 'synced';

    setDot('ntpDot', 'green');
    setVal('ntpVal', `Synced (${sign}${offsetMs}ms)`);
    setVal('syncCount', syncCount);

    log(`[NTP] Offset = ${sign}${offsetMs}ms`, 'ok');
}

// ── Connect ────────────────────────────────────────────────
function connect() {
    if (ws) return;

    const url = `ws://${window.location.hostname}:8080`;
    log(`Connecting to ${url}...`);

    document.getElementById('connectBtn').disabled = true;
    setDot('wsDot', 'yellow');
    setVal('wsVal', 'Connecting...');

    ws = new WebSocket(url);

    ws.onopen = () => {
        log('WebSocket connected!', 'ok');

        setDot('wsDot', 'green');
        setVal('wsVal', 'Connected');

        document.getElementById('connectBtn').textContent = 'Connected ✓';

        sendNtpSync();
        syncTimer = setInterval(sendNtpSync, 2000);
    };

    ws.onmessage = (event) => {
        let msg;
        try { msg = JSON.parse(event.data); }
        catch { return; }

        if (msg.type === 'welcome') {
            log(`Server: ${msg.msg}`);
        } else if (msg.type === 'ntp_response') {
            handleNtpResponse(msg);
        }
    };

    ws.onerror = () => {
        log('Connection error!', 'err');
        setDot('wsDot', 'red');
        setVal('wsVal', 'Error');
    };

    ws.onclose = () => {
        log('Disconnected.', 'err');

        setDot('wsDot', 'red');
        setVal('wsVal', 'Disconnected');

        setDot('ntpDot', '');
        clearInterval(syncTimer);

        ws = null;
        ntpSynced = false;

        document.getElementById('connectBtn').disabled = false;
        document.getElementById('connectBtn').textContent = 'Connect to Server';

        document.getElementById('offsetDisplay').textContent = '—';
        document.getElementById('offsetDisplay').className = '';
    };
}