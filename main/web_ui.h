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
  .can-motor-item {
    background: #12141c;
    border-radius: 6px;
    padding: 10px;
    margin-bottom: 6px;
  }
  .can-motor-header {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 6px;
  }
  .can-motor-id {
    font-family: 'SF Mono', monospace;
    font-size: 0.95em;
    font-weight: bold;
    color: #7eb8ff;
  }
  .can-motor-badge {
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 0.7em;
    font-weight: bold;
    text-transform: uppercase;
  }
  .can-motor-badge.run { background: #1b4332; color: #44ff44; }
  .can-motor-badge.cal { background: #4a3400; color: #ffaa00; }
  .can-motor-badge.rst { background: #333; color: #888; }
  .can-motor-badge.fault { background: #4a0000; color: #ff4444; }
  .can-motor-badge.stale { background: #3a3a00; color: #ccaa00; }
  .can-motor-item.stale { opacity: 0.5; border: 1px solid #665500; }
  .can-motor-role {
    padding: 2px 6px;
    border-radius: 3px;
    font-size: 0.7em;
    font-weight: bold;
    font-family: 'SF Mono', monospace;
    background: #2a4a7a;
    color: #7eb8ff;
  }
  .settings-link {
    float: right;
    font-size: 0.7em;
    color: #666;
    text-decoration: none;
  }
  .settings-link:hover { color: #7eb8ff; }
  .nav-bar {
    display: flex;
    gap: 12px;
    margin-bottom: 16px;
  }
  .nav-bar a {
    color: #7eb8ff;
    text-decoration: none;
    font-size: 0.9em;
    padding: 6px 14px;
    border-radius: 6px;
    background: #1a1d27;
    border: 1px solid #2a2d37;
    transition: background 0.2s;
  }
  .nav-bar a:hover {
    background: #2a4a7a;
  }
  .can-motor-stats {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 4px;
  }
  .can-motor-stat .label {
    font-size: 0.7em;
    color: #666;
  }
  .can-motor-stat .value {
    font-family: 'SF Mono', monospace;
    font-size: 0.9em;
    color: #fff;
  }
  #poll-status {
    font-size: 0.8em;
    color: #666;
    text-align: center;
    margin-top: 12px;
  }
  .robot-diagram {
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .robot-diagram svg {
    max-width: 240px;
    width: 100%;
  }
  .robot-diagram-legend {
    display: flex;
    gap: 16px;
    justify-content: center;
    margin-top: 8px;
    font-size: 0.8em;
  }
  .robot-diagram-legend span {
    display: flex;
    align-items: center;
    gap: 4px;
    color: #999;
  }
  .robot-diagram-legend .swatch {
    width: 12px;
    height: 4px;
    border-radius: 2px;
  }
  .robot-diagram-pos {
    display: flex;
    gap: 24px;
    justify-content: center;
    margin-top: 6px;
    font-family: 'SF Mono', monospace;
    font-size: 0.85em;
    color: #ccc;
  }
</style>
</head>
<body>
  <h1>
    <span class="status-dot" id="poll-dot"></span>
    JumpRopeStick
  </h1>

  <div class="nav-bar">
    <a href="/settings">Settings</a>
    <a href="/log">Log Viewer</a>
  </div>

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

  <div class="card" id="can-card">
    <h2>CAN Motors <span id="can-count">(0)</span> <a href="/settings" class="settings-link">Settings</a></h2>
    <div id="can-content" style="color:#555;text-align:center;padding:10px;font-style:italic;">No motors detected</div>
  </div>

  <div class="card">
    <h2>Robot Position</h2>
    <div class="robot-diagram">
      <svg viewBox="0 0 220 220" xmlns="http://www.w3.org/2000/svg">
        <!-- Reference circle -->
        <circle cx="110" cy="110" r="75" fill="none" stroke="#2a2d37" stroke-width="1" stroke-dasharray="4,4"/>
        <!-- Crosshairs -->
        <line x1="110" y1="35" x2="110" y2="185" stroke="#1e2130" stroke-width="1"/>
        <line x1="35" y1="110" x2="185" y2="110" stroke="#1e2130" stroke-width="1"/>
        <!-- Cardinal labels -->
        <text x="196" y="114" fill="#666" font-size="10" font-family="sans-serif" text-anchor="start">Front</text>
        <text x="24" y="114" fill="#666" font-size="10" font-family="sans-serif" text-anchor="end">Back</text>
        <text x="110" y="24" fill="#666" font-size="10" font-family="sans-serif" text-anchor="middle">Up</text>
        <text x="110" y="206" fill="#666" font-size="10" font-family="sans-serif" text-anchor="middle">Down</text>
        <!-- Left arm (orange, thicker) -->
        <line id="arm-left" x1="110" y1="110" x2="185" y2="110" stroke="#e8963e" stroke-width="6" stroke-linecap="round" opacity="0.9"/>
        <!-- Right arm (blue, thinner) -->
        <line id="arm-right" x1="110" y1="110" x2="185" y2="110" stroke="#7eb8ff" stroke-width="4" stroke-linecap="round" opacity="0.9"/>
        <!-- Robot body -->
        <rect x="97" y="99" width="26" height="22" rx="4" fill="#1a1d27" stroke="#555" stroke-width="1.5"/>
        <!-- Forward arrow on body -->
        <polygon points="123,110 118,106 118,114" fill="#666"/>
      </svg>
      <div class="robot-diagram-legend">
        <span><span class="swatch" style="background:#e8963e"></span> Left</span>
        <span><span class="swatch" style="background:#7eb8ff"></span> Right</span>
      </div>
      <div class="robot-diagram-pos">
        <span id="arm-left-pos">L: -- rad</span>
        <span id="arm-right-pos">R: -- rad</span>
      </div>
    </div>
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

    if (d.motors !== undefined) {
      setText('can-count', '(' + d.motors.length + ')');
      var canContent = document.getElementById('can-content');
      if (d.motors.length === 0) {
        var noMotorMsg = 'No motors detected';
        if (d.canRunning === false) {
          noMotorMsg = 'CAN bus not running';
        }
        canContent.innerHTML = '<div style="color:#555;text-align:center;padding:10px;font-style:italic;">' + noMotorMsg + '</div>';
      } else {
        var mHtml = '';
        for (var mi = 0; mi < d.motors.length; mi++) {
          mHtml += renderCanMotor(d.motors[mi]);
        }
        canContent.innerHTML = mHtml;
      }
    }

    updateRobotDiagram(d);

    if (d.drive) {
      updateMotorBar('l', parseFloat(d.drive.leftDrive) || 0, d.drive.left || 0);
      updateMotorBar('r', parseFloat(d.drive.rightDrive) || 0, d.drive.right || 0);
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

  function renderCanMotor(m) {
    var modeNames = ['Reset', 'Calibration', 'Running'];
    var modeClasses = ['rst', 'cal', 'run'];
    var modeIdx = m.mode || 0;
    if (modeIdx > 2) modeIdx = 0;
    var badgeClass = modeClasses[modeIdx];
    var badgeText = modeNames[modeIdx];
    if (m.hasFault) {
      badgeClass = 'fault';
      badgeText = 'FAULT 0x' + (m.errorCode || 0).toString(16).toUpperCase();
    }
    if (m.stale) {
      badgeClass = 'stale';
      badgeText = 'STALE';
    }

    var itemClass = 'can-motor-item';
    if (m.stale) {
      itemClass += ' stale';
    }

    var runModeNames = ['MIT', 'Pos-PP', 'Speed', 'Current', 'ZeroCal', 'Pos-CSP'];
    var runModeStr = runModeNames[m.runMode] || ('Mode ' + m.runMode);

    var voltage = parseFloat(m.voltage) || 0;
    var voltageStr = voltage > 0.1 ? voltage.toFixed(1) + 'V' : '--';

    var roleHtml = '';
    if (m.role === 'L') {
      roleHtml = '<span class="can-motor-role">LEFT</span>';
    } else if (m.role === 'R') {
      roleHtml = '<span class="can-motor-role">RIGHT</span>';
    }

    return '<div class="' + itemClass + '">' +
      '<div class="can-motor-header">' +
        '<span class="can-motor-id">Motor ' + m.id + '</span>' +
        roleHtml +
        '<span class="can-motor-badge ' + badgeClass + '">' + badgeText + '</span>' +
      '</div>' +
      '<div class="can-motor-stats">' +
        '<div class="can-motor-stat"><div class="label">Voltage</div><div class="value">' + voltageStr + '</div></div>' +
        '<div class="can-motor-stat"><div class="label">Position</div><div class="value">' + parseFloat(m.position).toFixed(2) + ' rad</div></div>' +
        '<div class="can-motor-stat"><div class="label">Velocity</div><div class="value">' + parseFloat(m.velocity).toFixed(1) + ' rad/s</div></div>' +
        '<div class="can-motor-stat"><div class="label">Torque</div><div class="value">' + parseFloat(m.torque).toFixed(2) + ' Nm</div></div>' +
        '<div class="can-motor-stat"><div class="label">Temp</div><div class="value">' + parseFloat(m.temperature).toFixed(0) + '&deg;C</div></div>' +
        '<div class="can-motor-stat"><div class="label">PP Speed</div><div class="value">' + parseFloat(m.ppSpeed || 0).toFixed(1) + ' rad/s</div></div>' +
        '<div class="can-motor-stat"><div class="label">PP Accel</div><div class="value">' + parseFloat(m.ppAccel || 0).toFixed(0) + ' rad/s&sup2;</div></div>' +
        '<div class="can-motor-stat"><div class="label">Limit Spd</div><div class="value">' + parseFloat(m.limitSpd || 0).toFixed(1) + ' rad/s</div></div>' +
        '<div class="can-motor-stat"><div class="label">Limit Cur</div><div class="value">' + parseFloat(m.limitCur || 0).toFixed(1) + ' A</div></div>' +
        '<div class="can-motor-stat"><div class="label">Mode</div><div class="value">' + runModeStr + '</div></div>' +
      '</div>' +
    '</div>';
  }

  function updateRobotDiagram(d) {
    if (!d.motors || d.motors.length === 0) return;
    var leftMotor = null;
    var rightMotor = null;
    // Try to find motors by explicit role assignment
    for (var i = 0; i < d.motors.length; i++) {
      if (d.motors[i].role === 'L') {
        leftMotor = d.motors[i];
      }
      if (d.motors[i].role === 'R') {
        rightMotor = d.motors[i];
      }
    }
    // Fallback: use motor index 0 as left, index 1 as right
    // (matches firmware resolveLeftMotorId / resolveRightMotorId behaviour)
    if (!leftMotor && d.motors.length >= 1) {
      leftMotor = d.motors[0];
    }
    if (!rightMotor && d.motors.length >= 2) {
      rightMotor = d.motors[1];
    }
    // Calibrated scale: 180° per 3.54 motor-rad so "Back" preset (3.54) = straight back
    // and "Up" preset (1.79) maps to ~91° which is effectively straight up.
    var MOTOR_RAD_TO_DEG = 180.0 / 3.54;
    var armLeft = document.getElementById('arm-left');
    var armRight = document.getElementById('arm-right');
    if (leftMotor && armLeft) {
      var leftDeg = parseFloat(leftMotor.position) * MOTOR_RAD_TO_DEG;
      armLeft.setAttribute('transform', 'rotate(' + leftDeg + ', 110, 110)');
      setText('arm-left-pos', 'L: ' + parseFloat(leftMotor.position).toFixed(2) + ' rad');
    }
    if (rightMotor && armRight) {
      var rightDeg = -parseFloat(rightMotor.position) * MOTOR_RAD_TO_DEG;
      armRight.setAttribute('transform', 'rotate(' + rightDeg + ', 110, 110)');
      setText('arm-right-pos', 'R: ' + parseFloat(rightMotor.position).toFixed(2) + ' rad');
    }
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

  // Poll at 4Hz
  setInterval(poll, 250);
  poll();
})();
</script>
</body>
</html>
)rawliteral";
