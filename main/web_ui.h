#pragma once

// =============================================================================
// Embedded Web UI - Polling-based status dashboard
// =============================================================================

static const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>JumpRopeStick</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #0f1117;
    color: #e0e0e0;
    min-height: 100vh;
    padding: 16px;
  }
  h1 {
    font-size: 1.4em;
    color: #7eb8ff;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .status-dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: #ff4444;
    display: inline-block;
    transition: background 0.3s;
  }
  .status-dot.ok { background: #44ff44; }
  .card {
    background: #1a1d27;
    border-radius: 8px;
    padding: 14px;
    margin-bottom: 12px;
    border: 1px solid #2a2d37;
  }
  .card h2 {
    font-size: 0.85em;
    color: #888;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 10px;
  }
  .row {
    display: flex;
    justify-content: space-between;
    padding: 4px 0;
    font-size: 0.95em;
  }
  .row .label { color: #999; }
  .row .value { font-family: 'SF Mono', monospace; color: #fff; }
  .controller-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .stick-display {
    background: #12141c;
    border-radius: 6px;
    padding: 10px;
    text-align: center;
  }
  .stick-display .label {
    font-size: 0.75em;
    color: #666;
    margin-bottom: 4px;
  }
  .stick-canvas {
    width: 100px;
    height: 100px;
    border-radius: 50%;
    background: #1e2130;
    border: 1px solid #333;
    position: relative;
    margin: 0 auto;
  }
  .stick-dot {
    width: 12px; height: 12px;
    border-radius: 50%;
    background: #7eb8ff;
    position: absolute;
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    transition: top 0.05s, left 0.05s;
  }
  .trigger-bar {
    height: 6px;
    background: #1e2130;
    border-radius: 3px;
    margin-top: 4px;
    overflow: hidden;
  }
  .trigger-fill {
    height: 100%;
    background: #7eb8ff;
    border-radius: 3px;
    width: 0%;
    transition: width 0.05s;
  }
  .no-controller {
    color: #555;
    text-align: center;
    padding: 20px;
    font-style: italic;
  }
  .btn-grid {
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
    margin-top: 8px;
  }
  .btn-indicator {
    padding: 3px 8px;
    border-radius: 4px;
    font-size: 0.75em;
    font-family: monospace;
    background: #1e2130;
    color: #555;
    border: 1px solid #333;
  }
  .btn-indicator.active {
    background: #2a4a7a;
    color: #7eb8ff;
    border-color: #7eb8ff;
  }
  .motor-row {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 6px 0;
  }
  .motor-label {
    width: 55px;
    font-size: 0.85em;
    color: #999;
    flex-shrink: 0;
  }
  .motor-bar-track {
    flex: 1;
    height: 14px;
    background: #1e2130;
    border-radius: 4px;
    position: relative;
    overflow: hidden;
  }
  .motor-bar-center {
    position: absolute;
    left: 50%;
    top: 0;
    bottom: 0;
    width: 1px;
    background: #444;
  }
  .motor-bar-fill {
    position: absolute;
    top: 1px;
    bottom: 1px;
    border-radius: 3px;
    transition: left 0.05s, width 0.05s, background 0.05s;
  }
  .motor-value {
    width: 45px;
    text-align: right;
    font-family: 'SF Mono', monospace;
    font-size: 0.8em;
    color: #fff;
    flex-shrink: 0;
  }
  .motor-us {
    font-size: 0.75em;
    color: #666;
    text-align: right;
    padding-right: 2px;
  }
  #poll-status {
    font-size: 0.8em;
    color: #666;
    text-align: center;
    margin-top: 12px;
  }
</style>
</head>
<body>
  <h1>
    <span class="status-dot" id="poll-dot"></span>
    JumpRopeStick
  </h1>

  <div class="card">
    <h2>WiFi</h2>
    <div class="row"><span class="label">SSID</span><span class="value" id="wifi-ssid">--</span></div>
    <div class="row"><span class="label">IP</span><span class="value" id="wifi-ip">--</span></div>
    <div class="row"><span class="label">RSSI</span><span class="value" id="wifi-rssi">--</span></div>
  </div>

  <div class="card" id="ctrl-card">
    <h2>Controller <span id="ctrl-count">(0)</span></h2>
    <div id="ctrl-content" class="no-controller">No controller connected. Put 8BitDo in pairing mode.</div>
  </div>

  <div class="card">
    <h2>Drive Output</h2>
    <div class="motor-row">
      <span class="motor-label">Left</span>
      <div class="motor-bar-track">
        <div class="motor-bar-center"></div>
        <div class="motor-bar-fill" id="motor-l-fill"></div>
      </div>
      <span class="motor-value" id="motor-l-val">0%</span>
    </div>
    <div class="motor-us" id="motor-l-us">1500 us</div>
    <div class="motor-row">
      <span class="motor-label">Right</span>
      <div class="motor-bar-track">
        <div class="motor-bar-center"></div>
        <div class="motor-bar-fill" id="motor-r-fill"></div>
      </div>
      <span class="motor-value" id="motor-r-val">0%</span>
    </div>
    <div class="motor-us" id="motor-r-us">1500 us</div>
  </div>

  <div class="card">
    <h2>System</h2>
    <div class="row"><span class="label">Uptime</span><span class="value" id="sys-uptime">0s</span></div>
    <div class="row"><span class="label">Free Heap</span><span class="value" id="sys-heap">--</span></div>
    <div class="row"><span class="label">Free PSRAM</span><span class="value" id="sys-psram">--</span></div>
  </div>

  <div id="poll-status">Starting...</div>

<script>
(function() {
  var dot = document.getElementById('poll-dot');
  var statusEl = document.getElementById('poll-status');
  var ok = false;
  var fails = 0;

  function poll() {
    fetch('/status')
      .then(function(r) { return r.json(); })
      .then(function(d) {
        ok = true;
        fails = 0;
        dot.classList.add('ok');
        statusEl.textContent = 'Connected';
        updateUI(d);
      })
      .catch(function() {
        fails++;
        if (fails > 3) {
          ok = false;
          dot.classList.remove('ok');
          statusEl.textContent = 'Connection lost...';
        }
      });
  }

  function updateUI(d) {
    if (d.wifi) {
      setText('wifi-ssid', d.wifi.ssid || '--');
      setText('wifi-ip', d.wifi.ip || '--');
      var rssiVal = d.wifi.rssi;
      setText('wifi-rssi', rssiVal ? (rssiVal + ' dBm') : '--');
    }

    if (d.controllers) {
      var connected = d.controllers.filter(function(c) { return c.connected; });
      setText('ctrl-count', '(' + connected.length + ')');

      var content = document.getElementById('ctrl-content');
      if (connected.length === 0) {
        content.className = 'no-controller';
        content.innerHTML = 'No controller connected. Put 8BitDo in pairing mode.';
      } else {
        content.className = '';
        var html = '';
        for (var i = 0; i < connected.length; i++) {
          html += renderController(connected[i]);
        }
        content.innerHTML = html;
        for (var j = 0; j < connected.length; j++) {
          updateStickDot('lstick-' + connected[j].id, connected[j].lx, connected[j].ly);
          updateStickDot('rstick-' + connected[j].id, connected[j].rx, connected[j].ry);
        }
      }
    }

    if (d.servos) {
      updateMotorBar('l', parseFloat(d.servos.leftDrive) || 0, d.servos.left || 1500);
      updateMotorBar('r', parseFloat(d.servos.rightDrive) || 0, d.servos.right || 1500);
    }

    if (d.system) {
      var up = d.system.uptime_s;
      var h = Math.floor(up / 3600);
      var m = Math.floor((up % 3600) / 60);
      var s = up % 60;
      var upStr = '';
      if (h > 0) upStr += h + 'h ';
      if (m > 0 || h > 0) upStr += m + 'm ';
      upStr += s + 's';
      setText('sys-uptime', upStr);
      setText('sys-heap', formatBytes(d.system.free_heap));
      setText('sys-psram', formatBytes(d.system.free_psram));
    }
  }

  function renderController(c) {
    var l2Pct = Math.round((c.l2 / 1023) * 100);
    var r2Pct = Math.round((c.r2 / 1023) * 100);

    var btnNames = ['A','B','X','Y','L1','R1','L3','R3'];
    var btnHtml = '';
    for (var b = 0; b < btnNames.length; b++) {
      var active = (c.buttons & (1 << b)) ? ' active' : '';
      btnHtml += '<span class="btn-indicator' + active + '">' + btnNames[b] + '</span>';
    }
    var dpadNames = [{n:'U',m:1},{n:'D',m:2},{n:'L',m:8},{n:'R',m:4}];
    for (var dp = 0; dp < dpadNames.length; dp++) {
      var dpActive = (c.dpad & dpadNames[dp].m) ? ' active' : '';
      btnHtml += '<span class="btn-indicator' + dpActive + '">' + dpadNames[dp].n + '</span>';
    }

    return '<div style="margin-bottom:8px"><div style="font-size:0.85em;color:#aaa;margin-bottom:6px">' +
      (c.model || ('Gamepad ' + c.id)) + '</div>' +
      '<div class="controller-grid">' +
      '<div class="stick-display"><div class="label">Left Stick</div>' +
      '<div class="stick-canvas" id="lstick-' + c.id + '"><div class="stick-dot"></div></div>' +
      '<div style="font-size:0.7em;color:#666;margin-top:4px">' + c.lx + ', ' + c.ly + '</div></div>' +
      '<div class="stick-display"><div class="label">Right Stick</div>' +
      '<div class="stick-canvas" id="rstick-' + c.id + '"><div class="stick-dot"></div></div>' +
      '<div style="font-size:0.7em;color:#666;margin-top:4px">' + c.rx + ', ' + c.ry + '</div></div>' +
      '</div>' +
      '<div style="margin-top:8px">' +
      '<div class="row"><span class="label">L2</span><span class="value">' + c.l2 + '</span></div>' +
      '<div class="trigger-bar"><div class="trigger-fill" style="width:' + l2Pct + '%"></div></div>' +
      '<div class="row"><span class="label">R2</span><span class="value">' + c.r2 + '</span></div>' +
      '<div class="trigger-bar"><div class="trigger-fill" style="width:' + r2Pct + '%"></div></div>' +
      '</div>' +
      '<div class="btn-grid">' + btnHtml + '</div></div>';
  }

  function updateStickDot(canvasId, x, y) {
    var canvas = document.getElementById(canvasId);
    if (!canvas) return;
    var dot = canvas.querySelector('.stick-dot');
    if (!dot) return;
    var px = 50 + (x / 512) * 40;
    var py = 50 + (y / 512) * 40;
    dot.style.left = px + '%';
    dot.style.top = py + '%';
  }

  function updateMotorBar(side, drive, pulseUs) {
    var fill = document.getElementById('motor-' + side + '-fill');
    var valEl = document.getElementById('motor-' + side + '-val');
    var usEl = document.getElementById('motor-' + side + '-us');
    if (!fill || !valEl || !usEl) return;

    var pct = Math.round(Math.abs(drive) * 100);
    valEl.textContent = (drive >= 0 ? '+' : '-') + pct + '%';
    usEl.textContent = pulseUs + ' us';

    // Bar grows from center (50%) outward
    var widthPct = Math.abs(drive) * 50;
    if (drive >= 0) {
      fill.style.left = '50%';
      fill.style.width = widthPct + '%';
      fill.style.background = '#4caf50';
    } else {
      fill.style.left = (50 - widthPct) + '%';
      fill.style.width = widthPct + '%';
      fill.style.background = '#ff5722';
    }
  }

  function setText(id, val) {
    var el = document.getElementById(id);
    if (el) el.textContent = val;
  }

  function formatBytes(b) {
    if (b === undefined || b === null) return '--';
    if (b > 1024 * 1024) return (b / (1024*1024)).toFixed(1) + ' MB';
    if (b > 1024) return (b / 1024).toFixed(1) + ' KB';
    return b + ' B';
  }

  // Poll at 10Hz
  setInterval(poll, 100);
  poll();
})();
</script>
</body>
</html>
)rawliteral";
