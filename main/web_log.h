#pragma once

// =============================================================================
// Embedded Web Log Viewer - Streams debug logs with IMU/state telemetry
// =============================================================================

static const char WEB_LOG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>JumpRopeStick - Log</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: 'SF Mono', 'Fira Code', 'Cascadia Code', 'Consolas', monospace;
    background: #0f1117;
    color: #c8ccd4;
    height: 100vh;
    display: flex;
    flex-direction: column;
  }

  /* Top bar */
  .topbar {
    background: #1a1d27;
    border-bottom: 1px solid #2a2d3a;
    padding: 10px 16px;
    display: flex;
    align-items: center;
    gap: 12px;
    flex-wrap: wrap;
    position: sticky;
    top: 0;
    z-index: 10;
  }
  .topbar h1 {
    font-size: 15px;
    font-weight: 600;
    color: #e0e0e0;
    white-space: nowrap;
  }
  .topbar .status {
    font-size: 12px;
    color: #6b7280;
    flex: 1;
    min-width: 120px;
  }
  .topbar .status .dot {
    display: inline-block;
    width: 8px; height: 8px;
    border-radius: 50%;
    background: #ef4444;
    margin-right: 4px;
    vertical-align: middle;
  }
  .topbar .status .dot.ok { background: #22c55e; }

  .btn {
    font-size: 12px;
    padding: 5px 12px;
    border: 1px solid #3a3d4a;
    border-radius: 6px;
    background: #22252f;
    color: #c8ccd4;
    cursor: pointer;
    white-space: nowrap;
    transition: background 0.15s;
  }
  .btn:hover { background: #2d3040; }
  .btn.primary { background: #2563eb; border-color: #3b82f6; color: #fff; }
  .btn.primary:hover { background: #1d4ed8; }
  .btn.danger { background: #7f1d1d; border-color: #991b1b; color: #fca5a5; }
  .btn.danger:hover { background: #991b1b; }

  /* Telemetry bar */
  .telemetry {
    background: #14161e;
    border-bottom: 1px solid #2a2d3a;
    padding: 8px 16px;
    display: flex;
    gap: 20px;
    flex-wrap: wrap;
    font-size: 12px;
    position: sticky;
    top: 48px;
    z-index: 9;
  }
  .telem-item {
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .telem-label {
    color: #6b7280;
  }
  .telem-val {
    color: #a5b4fc;
    font-weight: 600;
    min-width: 50px;
  }
  .telem-val.warn { color: #fbbf24; }
  .telem-val.alert { color: #f87171; }
  .telem-val.ok { color: #4ade80; }

  /* Filter bar */
  .filterbar {
    background: #14161e;
    border-bottom: 1px solid #2a2d3a;
    padding: 6px 16px;
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 12px;
  }
  .filterbar label { color: #6b7280; }
  .filterbar input[type="text"] {
    background: #1a1d27;
    border: 1px solid #3a3d4a;
    border-radius: 4px;
    color: #c8ccd4;
    padding: 3px 8px;
    font-family: inherit;
    font-size: 12px;
    width: 200px;
  }
  .filterbar input[type="text"]:focus {
    outline: none;
    border-color: #3b82f6;
  }

  /* Log area */
  .log-container {
    flex: 1;
    overflow-y: auto;
    padding: 8px 0;
  }
  .log-line {
    padding: 1px 16px;
    white-space: pre;
    font-size: 12px;
    line-height: 1.6;
    border-left: 3px solid transparent;
  }
  .log-line:hover {
    background: #1a1d27;
  }
  .log-line.level-ERROR {
    color: #f87171;
    border-left-color: #ef4444;
  }
  .log-line.level-WARN {
    color: #fbbf24;
    border-left-color: #f59e0b;
  }
  .log-line.level-INFO {
    color: #c8ccd4;
  }
  .log-line.level-DEBUG {
    color: #6b7280;
  }
  .log-line .tag {
    color: #60a5fa;
  }
  .log-line .ts {
    color: #6b7280;
  }
  .log-line.hidden {
    display: none;
  }

  /* Bottom status */
  .bottombar {
    background: #1a1d27;
    border-top: 1px solid #2a2d3a;
    padding: 6px 16px;
    font-size: 11px;
    color: #6b7280;
    display: flex;
    justify-content: space-between;
    position: sticky;
    bottom: 0;
  }

  /* Responsive */
  @media (max-width: 600px) {
    .telemetry { gap: 10px; }
    .telem-item { font-size: 11px; }
  }
</style>
</head>
<body>

<div class="topbar">
  <h1>Log Viewer</h1>
  <div class="status">
    <span class="dot" id="connDot"></span>
    <span id="connText">Connecting...</span>
  </div>
  <button class="btn" id="btnPause">Pause</button>
  <button class="btn" id="btnScroll">Auto-scroll: ON</button>
  <button class="btn primary" id="btnCopy">Copy All</button>
  <button class="btn danger" id="btnClear">Clear</button>
  <a href="/" class="btn">Dashboard</a>
</div>

<div class="telemetry" id="telemetry">
  <div class="telem-item"><span class="telem-label">Pitch:</span><span class="telem-val" id="tPitch">--</span></div>
  <div class="telem-item"><span class="telem-label">Flipped:</span><span class="telem-val" id="tFlipped">--</span></div>
  <div class="telem-item"><span class="telem-label">SelfRight:</span><span class="telem-val" id="tSR">--</span></div>
  <div class="telem-item"><span class="telem-label">NoseDown:</span><span class="telem-val" id="tND">--</span></div>
  <div class="telem-item"><span class="telem-label">DriveL:</span><span class="telem-val" id="tDriveL">--</span></div>
  <div class="telem-item"><span class="telem-label">DriveR:</span><span class="telem-val" id="tDriveR">--</span></div>
  <div class="telem-item"><span class="telem-label">Uptime:</span><span class="telem-val" id="tUptime">--</span></div>
</div>

<div class="filterbar">
  <label>Filter:</label>
  <input type="text" id="filterInput" placeholder="e.g. NoseDown, PID, error..." />
  <span id="filterCount" style="color:#6b7280"></span>
</div>

<div class="log-container" id="logContainer"></div>

<div class="bottombar">
  <span id="lineCount">0 lines</span>
  <span id="pollInfo">--</span>
</div>

<script>
(function() {
  // State
  let logLines = [];
  let lastSeq = 0;
  let paused = false;
  let autoScroll = true;
  let pollCount = 0;
  let pollErrors = 0;
  let filterText = '';

  // DOM refs
  const container = document.getElementById('logContainer');
  const connDot = document.getElementById('connDot');
  const connText = document.getElementById('connText');
  const lineCountEl = document.getElementById('lineCount');
  const pollInfoEl = document.getElementById('pollInfo');
  const filterCountEl = document.getElementById('filterCount');

  // Telemetry refs
  const tPitch = document.getElementById('tPitch');
  const tFlipped = document.getElementById('tFlipped');
  const tSR = document.getElementById('tSR');
  const tND = document.getElementById('tND');
  const tDriveL = document.getElementById('tDriveL');
  const tDriveR = document.getElementById('tDriveR');
  const tUptime = document.getElementById('tUptime');

  const SR_NAMES = ['IDLE', 'PREP', 'PUSH', 'DONE'];
  const ND_NAMES = ['IDLE', 'SELF_RIGHT', 'TIPPING', 'BALANCING', 'EXITING'];

  function detectLevel(text) {
    if (text.indexOf('ERROR') !== -1) return 'ERROR';
    if (text.indexOf('WARN') !== -1) return 'WARN';
    if (text.indexOf('DEBUG') !== -1) return 'DEBUG';
    return 'INFO';
  }

  function escapeHtml(str) {
    return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
  }

  function formatLogLine(text) {
    // Highlight tag in brackets: [TagName]
    let escaped = escapeHtml(text);
    // Highlight timestamp
    escaped = escaped.replace(/^(\[\s*\d+\])/, '<span class="ts">$1</span>');
    // Highlight tag
    escaped = escaped.replace(/(\[[\w\s]+\])(?!.*\[)/, '<span class="tag">$1</span>');
    return escaped;
  }

  function addLines(entries) {
    let fragment = document.createDocumentFragment();
    for (let i = 0; i < entries.length; i++) {
      let text = entries[i];
      logLines.push(text);
      let div = document.createElement('div');
      let level = detectLevel(text);
      div.className = 'log-line level-' + level;
      div.innerHTML = formatLogLine(text);
      div.dataset.idx = logLines.length - 1;
      // Apply filter
      if (filterText && text.toLowerCase().indexOf(filterText) === -1) {
        div.classList.add('hidden');
      }
      fragment.appendChild(div);
    }
    container.appendChild(fragment);
    lineCountEl.textContent = logLines.length + ' lines';
    updateFilterCount();
    if (autoScroll && !paused) {
      container.scrollTop = container.scrollHeight;
    }
  }

  function updateTelemetry(data) {
    if (data.pitch !== undefined) {
      let deg = (data.pitch * 180 / Math.PI).toFixed(1);
      tPitch.textContent = deg + '\u00B0';
      let absDeg = Math.abs(parseFloat(deg));
      tPitch.className = 'telem-val' + (absDeg > 70 ? ' alert' : (absDeg > 30 ? ' warn' : ''));
    }
    if (data.flipped !== undefined) {
      tFlipped.textContent = data.flipped ? 'YES' : 'no';
      tFlipped.className = 'telem-val' + (data.flipped ? ' alert' : ' ok');
    }
    if (data.sr !== undefined) {
      let name = SR_NAMES[data.sr] || '?';
      tSR.textContent = name;
      tSR.className = 'telem-val' + (data.sr > 0 ? ' warn' : '');
    }
    if (data.nd !== undefined) {
      let name = ND_NAMES[data.nd] || '?';
      tND.textContent = name;
      tND.className = 'telem-val' + (data.nd > 0 ? ' warn' : '');
    }
    if (data.driveL !== undefined) {
      tDriveL.textContent = data.driveL.toFixed(2);
    }
    if (data.driveR !== undefined) {
      tDriveR.textContent = data.driveR.toFixed(2);
    }
    if (data.uptime !== undefined) {
      let s = data.uptime;
      let m = Math.floor(s / 60);
      let sec = s % 60;
      tUptime.textContent = m + 'm' + sec + 's';
    }
  }

  async function poll() {
    if (paused) return;
    try {
      let resp = await fetch('/logs?since=' + lastSeq);
      if (!resp.ok) throw new Error('HTTP ' + resp.status);
      let data = await resp.json();
      connDot.classList.add('ok');
      connText.textContent = 'Connected';
      pollErrors = 0;

      if (data.entries && data.entries.length > 0) {
        addLines(data.entries);
      }
      if (data.head !== undefined) {
        lastSeq = data.head;
      }
      updateTelemetry(data);
      pollCount++;
      pollInfoEl.textContent = 'Poll #' + pollCount + ' | seq=' + lastSeq;
    } catch(e) {
      pollErrors++;
      connDot.classList.remove('ok');
      connText.textContent = 'Error (' + pollErrors + ')';
      pollInfoEl.textContent = 'Error: ' + e.message;
    }
  }

  function applyFilter() {
    filterText = document.getElementById('filterInput').value.toLowerCase().trim();
    let lines = container.querySelectorAll('.log-line');
    for (let i = 0; i < lines.length; i++) {
      let idx = parseInt(lines[i].dataset.idx);
      if (filterText && logLines[idx].toLowerCase().indexOf(filterText) === -1) {
        lines[i].classList.add('hidden');
      } else {
        lines[i].classList.remove('hidden');
      }
    }
    updateFilterCount();
  }

  function updateFilterCount() {
    if (!filterText) {
      filterCountEl.textContent = '';
      return;
    }
    let visible = container.querySelectorAll('.log-line:not(.hidden)').length;
    filterCountEl.textContent = visible + '/' + logLines.length + ' shown';
  }

  // Buttons
  document.getElementById('btnPause').addEventListener('click', function() {
    paused = !paused;
    this.textContent = paused ? 'Resume' : 'Pause';
    this.classList.toggle('primary', paused);
  });

  document.getElementById('btnScroll').addEventListener('click', function() {
    autoScroll = !autoScroll;
    this.textContent = 'Auto-scroll: ' + (autoScroll ? 'ON' : 'OFF');
  });

  document.getElementById('btnCopy').addEventListener('click', function() {
    let text;
    if (filterText) {
      text = logLines.filter(function(l) {
        return l.toLowerCase().indexOf(filterText) !== -1;
      }).join('\n');
    } else {
      text = logLines.join('\n');
    }
    navigator.clipboard.writeText(text).then(function() {
      let btn = document.getElementById('btnCopy');
      let orig = btn.textContent;
      btn.textContent = 'Copied!';
      setTimeout(function() { btn.textContent = orig; }, 1500);
    });
  });

  document.getElementById('btnClear').addEventListener('click', function() {
    logLines = [];
    container.innerHTML = '';
    lineCountEl.textContent = '0 lines';
    updateFilterCount();
  });

  document.getElementById('filterInput').addEventListener('input', applyFilter);

  // Poll loop - 4Hz
  setInterval(poll, 250);
  poll();
})();
</script>
</body>
</html>
)rawliteral";
