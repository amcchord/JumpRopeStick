#pragma once

// =============================================================================
// Embedded Web Config Page - Settings & Configuration
// =============================================================================

static const char WEB_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>JumpRopeStick - Settings</title>
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
  .back-link {
    color: #7eb8ff;
    text-decoration: none;
    font-size: 0.85em;
    display: inline-block;
    margin-bottom: 16px;
  }
  .back-link:hover { text-decoration: underline; }
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
  .card p.desc {
    font-size: 0.85em;
    color: #777;
    margin-bottom: 14px;
  }
  .form-row {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-bottom: 12px;
  }
  .form-row label {
    width: 120px;
    font-size: 0.9em;
    color: #999;
    flex-shrink: 0;
  }
  input[type="number"] {
    flex: 1;
    background: #12141c;
    color: #fff;
    border: 1px solid #333;
    border-radius: 6px;
    padding: 8px 12px;
    font-size: 0.9em;
    font-family: 'SF Mono', monospace;
  }
  input[type="number"]:focus {
    outline: none;
    border-color: #7eb8ff;
  }
  select {
    flex: 1;
    background: #12141c;
    color: #fff;
    border: 1px solid #333;
    border-radius: 6px;
    padding: 8px 12px;
    font-size: 0.9em;
    font-family: 'SF Mono', monospace;
    appearance: none;
    -webkit-appearance: none;
    cursor: pointer;
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23666' d='M6 8L1 3h10z'/%3E%3C/svg%3E");
    background-repeat: no-repeat;
    background-position: right 10px center;
    padding-right: 30px;
  }
  select:focus {
    outline: none;
    border-color: #7eb8ff;
  }
  .btn {
    background: #2a4a7a;
    color: #7eb8ff;
    border: 1px solid #7eb8ff;
    border-radius: 6px;
    padding: 10px 24px;
    font-size: 0.9em;
    cursor: pointer;
    transition: background 0.2s;
  }
  .btn:hover { background: #3a5a8a; }
  .btn:active { background: #1a3a6a; }
  .btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }
  .btn-row {
    display: flex;
    gap: 12px;
    margin-top: 16px;
  }
  .status-msg {
    font-size: 0.85em;
    margin-top: 12px;
    padding: 8px 12px;
    border-radius: 6px;
    display: none;
  }
  .status-msg.ok {
    display: block;
    background: #1b4332;
    color: #44ff44;
    border: 1px solid #2a5a42;
  }
  .status-msg.err {
    display: block;
    background: #4a0000;
    color: #ff4444;
    border: 1px solid #6a2020;
  }
  .ref-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.85em;
  }
  .ref-table th {
    text-align: left;
    color: #888;
    padding: 6px 8px;
    border-bottom: 1px solid #2a2d37;
    font-weight: normal;
    text-transform: uppercase;
    font-size: 0.8em;
    letter-spacing: 0.5px;
  }
  .ref-table td {
    padding: 6px 8px;
    border-bottom: 1px solid #1e2130;
  }
  .ref-table td:first-child {
    font-family: 'SF Mono', monospace;
    color: #7eb8ff;
    white-space: nowrap;
    width: 130px;
  }
  .ref-table td:nth-child(2) {
    color: #ccc;
  }
  .preset-group {
    background: #12141c;
    border-radius: 6px;
    padding: 12px;
    margin-bottom: 10px;
  }
  .preset-group h3 {
    font-size: 0.9em;
    color: #7eb8ff;
    margin-bottom: 8px;
    font-weight: 600;
  }
  .preset-group .form-row {
    margin-bottom: 8px;
  }
  .preset-group .form-row label {
    width: 90px;
  }
  .pos-fields {
    margin-top: 8px;
  }
  .ref-positions {
    font-size: 0.8em;
    color: #666;
    margin-top: 8px;
    padding: 10px 12px;
    background: #12141c;
    border-radius: 6px;
    line-height: 1.6;
  }
  .ref-positions code {
    color: #999;
    font-family: 'SF Mono', monospace;
  }
  .discovered-list {
    font-size: 0.8em;
    color: #666;
    margin-top: 8px;
    padding-top: 8px;
    border-top: 1px solid #2a2d37;
  }
  .discovered-list span {
    font-family: 'SF Mono', monospace;
    color: #999;
  }
  .mode-desc {
    font-size: 0.8em;
    color: #666;
    margin-top: 4px;
    font-style: italic;
  }
</style>
</head>
<body>
  <a href="/" class="back-link">&larr; Back to Dashboard</a>
  <h1>Settings</h1>

  <!-- ================================================================== -->
  <!-- BUTTON REFERENCE -->
  <!-- ================================================================== -->
  <div class="card">
    <h2>Controller Button Reference</h2>
    <table class="ref-table">
      <thead>
        <tr><th>Input</th><th>Action</th></tr>
      </thead>
      <tbody>
        <tr><td>Left Stick Y</td><td>Jog arms forward / back (velocity)</td></tr>
        <tr><td>Left Stick X</td><td>Differential arm spread</td></tr>
        <tr><td>R2 Trigger</td><td>Interpolate from home to trigger target</td></tr>
        <tr><td>R1</td><td>Fast drive mode (hold for full speed)</td></tr>
        <tr><td>L1</td><td>Cycle home presets (Front / Up / Back / L-Front/R-Back)</td></tr>
        <tr><td>L3</td><td>Smart return to home (nearest safe front)</td></tr>
        <tr><td>Y</td><td>Configurable action (see below)</td></tr>
        <tr><td>B</td><td>Configurable action (see below)</td></tr>
        <tr><td>A</td><td>Configurable action (see below)</td></tr>
        <tr><td>X</td><td>Nose-down balance mode</td></tr>
        <tr><td>Select</td><td>Self-righting sequence</td></tr>
        <tr><td>Sys</td><td>Set mechanical zero on both motors</td></tr>
        <tr><td>D-pad U/D</td><td>Trim left motor position</td></tr>
        <tr><td>D-pad L/R</td><td>Trim right motor position</td></tr>
      </tbody>
    </table>
  </div>

  <!-- ================================================================== -->
  <!-- BUTTON ACTIONS (Y / B / A) -->
  <!-- ================================================================== -->
  <div class="card">
    <h2>Button Actions (Y / B / A)</h2>
    <p class="desc">
      Configure what each button does when pressed.
      "Go to Position" moves arms to the specified radian values while the button is held.
      Other modes trigger animations on button press.
      All settings persist across reboots.
    </p>

    <div class="ref-positions">
      <strong>Reference positions (for "Go to Position" mode):</strong><br>
      Front: <code>L = 0.00, R = 0.00</code><br>
      Up: <code>L = -1.79, R = -1.79</code><br>
      Back: <code>L = -3.54, R = -3.54</code><br>
      Stand on Arms: <code>L = 0.65, R = -4.25</code><br>
      Arms Straight Down: <code>L = 1.37, R = -1.37</code>
    </div>

    <div style="margin-top: 14px;">
      <div class="preset-group">
        <h3>Y Button</h3>
        <div class="form-row">
          <label for="y-mode">Action</label>
          <select id="y-mode" onchange="togglePosFields('y')">
            <option value="0">Go to Position</option>
            <option value="1">Forward 360</option>
            <option value="2">Backward 360</option>
            <option value="3">Ground Slap</option>
          </select>
        </div>
        <div class="mode-desc" id="y-mode-desc"></div>
        <div class="pos-fields" id="y-pos-fields">
          <div class="form-row">
            <label for="y-left">Left (rad)</label>
            <input type="number" id="y-left" step="0.01" value="0">
          </div>
          <div class="form-row">
            <label for="y-right">Right (rad)</label>
            <input type="number" id="y-right" step="0.01" value="0">
          </div>
        </div>
      </div>

      <div class="preset-group">
        <h3>B Button</h3>
        <div class="form-row">
          <label for="b-mode">Action</label>
          <select id="b-mode" onchange="togglePosFields('b')">
            <option value="0">Go to Position</option>
            <option value="1">Forward 360</option>
            <option value="2">Backward 360</option>
            <option value="3">Ground Slap</option>
          </select>
        </div>
        <div class="mode-desc" id="b-mode-desc"></div>
        <div class="pos-fields" id="b-pos-fields">
          <div class="form-row">
            <label for="b-left">Left (rad)</label>
            <input type="number" id="b-left" step="0.01" value="0">
          </div>
          <div class="form-row">
            <label for="b-right">Right (rad)</label>
            <input type="number" id="b-right" step="0.01" value="0">
          </div>
        </div>
      </div>

      <div class="preset-group">
        <h3>A Button</h3>
        <div class="form-row">
          <label for="a-mode">Action</label>
          <select id="a-mode" onchange="togglePosFields('a')">
            <option value="0">Go to Position</option>
            <option value="1">Forward 360</option>
            <option value="2">Backward 360</option>
            <option value="3">Ground Slap</option>
          </select>
        </div>
        <div class="mode-desc" id="a-mode-desc"></div>
        <div class="pos-fields" id="a-pos-fields">
          <div class="form-row">
            <label for="a-left">Left (rad)</label>
            <input type="number" id="a-left" step="0.01" value="0">
          </div>
          <div class="form-row">
            <label for="a-right">Right (rad)</label>
            <input type="number" id="a-right" step="0.01" value="0">
          </div>
        </div>
      </div>
    </div>

    <div class="btn-row">
      <button class="btn" id="save-presets-btn" onclick="savePresets()">Save Button Actions</button>
    </div>
    <div class="status-msg" id="presets-status"></div>
  </div>

  <!-- ================================================================== -->
  <!-- MOTOR TUNING -->
  <!-- ================================================================== -->
  <div class="card">
    <h2>Motor Tuning</h2>
    <p class="desc">
      Configure PP-mode motor parameters. Changes take effect immediately on running motors.
    </p>

    <div class="form-row">
      <label for="speed-limit">Speed (rad/s)</label>
      <input type="number" id="speed-limit" step="0.1" min="0.1" max="50" value="25.0">
    </div>
    <div class="ref-positions" style="margin-bottom:10px;">
      Max velocity during profiled moves. Range: 0.1 &ndash; 50.0. Default: 25.0
    </div>

    <div class="form-row">
      <label for="acceleration">Accel (rad/s&sup2;)</label>
      <input type="number" id="acceleration" step="1" min="1" max="500" value="200">
    </div>
    <div class="ref-positions" style="margin-bottom:10px;">
      Acceleration / deceleration rate. Higher = snappier. Range: 1 &ndash; 500. Default: 200
    </div>

    <div class="form-row">
      <label for="current-limit">Current (A)</label>
      <input type="number" id="current-limit" step="0.5" min="0.5" max="40" value="23.0">
    </div>
    <div class="ref-positions" style="margin-bottom:10px;">
      Motor current limit (amps). Higher = more torque. Range: 0.5 &ndash; 40.0. Default: 23.0
    </div>

    <div class="btn-row">
      <button class="btn" id="save-motor-btn" onclick="saveMotorTuning()">Save Motor Tuning</button>
    </div>
    <div class="status-msg" id="speed-status"></div>
  </div>

  <!-- ================================================================== -->
  <!-- MOTOR ROLE ASSIGNMENT (CAN Motor IDs) -->
  <!-- ================================================================== -->
  <div class="card">
    <h2>Motor Role Assignment</h2>
    <p class="desc">
      Assign which CAN motor is the left side and which is the right side.
      These settings are saved to flash and persist across reboots.
    </p>

    <div class="form-row">
      <label for="left-motor">Left Motor</label>
      <select id="left-motor">
        <option value="0">-- None --</option>
      </select>
    </div>

    <div class="form-row">
      <label for="right-motor">Right Motor</label>
      <select id="right-motor">
        <option value="0">-- None --</option>
      </select>
    </div>

    <div class="discovered-list" id="discovered-info">
      Discovered motors: <span id="discovered-ids">loading...</span>
    </div>

    <div class="btn-row">
      <button class="btn" id="save-btn" onclick="saveConfig()">Save</button>
    </div>

    <div class="status-msg" id="config-status"></div>
  </div>

  <div class="card">
    <h2>Manual Motor ID Entry</h2>
    <p class="desc">
      If a motor is not currently connected, you can type its CAN ID directly (1-127).
    </p>
    <div class="form-row">
      <label for="manual-left">Left ID</label>
      <input type="number" id="manual-left" min="0" max="127" value="0">
    </div>
    <div class="form-row">
      <label for="manual-right">Right ID</label>
      <input type="number" id="manual-right" min="0" max="127" value="0">
    </div>
    <div class="btn-row">
      <button class="btn" onclick="saveManual()">Save Manual IDs</button>
    </div>
  </div>

<script>
(function() {
  // ---- Element refs ----
  var leftSelect = document.getElementById('left-motor');
  var rightSelect = document.getElementById('right-motor');
  var manualLeft = document.getElementById('manual-left');
  var manualRight = document.getElementById('manual-right');
  var configStatus = document.getElementById('config-status');
  var discoveredIds = document.getElementById('discovered-ids');
  var presetsStatus = document.getElementById('presets-status');
  var speedStatus = document.getElementById('speed-status');

  // Mode descriptions
  var modeDescs = [
    '',
    'Full forward rotation (+360 degrees) from current arm position.',
    'Full backward rotation (-360 degrees) from current arm position.',
    'Rapid oscillation near zero (-0.1 to +0.1 rad, 3 cycles).'
  ];

  // ---- Toggle position fields visibility based on mode ----
  window.togglePosFields = function(btn) {
    var modeVal = parseInt(document.getElementById(btn + '-mode').value);
    var posFields = document.getElementById(btn + '-pos-fields');
    var descEl = document.getElementById(btn + '-mode-desc');
    if (modeVal === 0) {
      posFields.style.display = '';
      descEl.textContent = '';
    } else {
      posFields.style.display = 'none';
      descEl.textContent = modeDescs[modeVal] || '';
    }
  };

  // ---- Load motor config (existing functionality) ----
  function loadMotorConfig() {
    fetch('/config')
      .then(function(r) { return r.json(); })
      .then(function(d) {
        populateDropdowns(d.discovered || [], d.leftId || 0, d.rightId || 0);
        manualLeft.value = d.leftId || 0;
        manualRight.value = d.rightId || 0;

        if (d.discovered && d.discovered.length > 0) {
          discoveredIds.textContent = d.discovered.join(', ');
        } else {
          discoveredIds.textContent = 'none';
        }
      })
      .catch(function(err) {
        showStatus(configStatus, 'Failed to load config: ' + err, false);
      });
  }

  // ---- Load settings (modes + presets + speed) ----
  function loadSettings() {
    fetch('/settingsdata')
      .then(function(r) { return r.json(); })
      .then(function(d) {
        // Set modes
        document.getElementById('y-mode').value = d.yMode || 0;
        document.getElementById('b-mode').value = d.bMode || 0;
        document.getElementById('a-mode').value = d.aMode || 0;

        // Set position values
        document.getElementById('y-left').value = d.yLeft;
        document.getElementById('y-right').value = d.yRight;
        document.getElementById('b-left').value = d.bLeft;
        document.getElementById('b-right').value = d.bRight;
        document.getElementById('a-left').value = d.aLeft;
        document.getElementById('a-right').value = d.aRight;
        document.getElementById('speed-limit').value = d.speedLimit;
        document.getElementById('acceleration').value = d.acceleration;
        document.getElementById('current-limit').value = d.currentLimit;

        // Update visibility
        togglePosFields('y');
        togglePosFields('b');
        togglePosFields('a');
      })
      .catch(function(err) {
        showStatus(presetsStatus, 'Failed to load settings: ' + err, false);
      });
  }

  function populateDropdowns(discovered, leftId, rightId) {
    var noneOpt = '<option value="0">-- None --</option>';
    var leftHtml = noneOpt;
    var rightHtml = noneOpt;

    for (var i = 0; i < discovered.length; i++) {
      var id = discovered[i];
      leftHtml += '<option value="' + id + '">Motor ' + id + '</option>';
      rightHtml += '<option value="' + id + '">Motor ' + id + '</option>';
    }

    if (leftId > 0) {
      var leftFound = false;
      for (var j = 0; j < discovered.length; j++) {
        if (discovered[j] === leftId) { leftFound = true; break; }
      }
      if (!leftFound) {
        leftHtml += '<option value="' + leftId + '">Motor ' + leftId + ' (offline)</option>';
      }
    }
    if (rightId > 0) {
      var rightFound = false;
      for (var k = 0; k < discovered.length; k++) {
        if (discovered[k] === rightId) { rightFound = true; break; }
      }
      if (!rightFound) {
        rightHtml += '<option value="' + rightId + '">Motor ' + rightId + ' (offline)</option>';
      }
    }

    leftSelect.innerHTML = leftHtml;
    rightSelect.innerHTML = rightHtml;
    leftSelect.value = leftId;
    rightSelect.value = rightId;
  }

  function showStatus(el, msg, success) {
    el.textContent = msg;
    el.className = 'status-msg ' + (success ? 'ok' : 'err');
    if (success) {
      setTimeout(function() { el.className = 'status-msg'; }, 3000);
    }
  }

  // ---- Save button actions (modes + positions) ----
  window.savePresets = function() {
    var body = {
      yMode:  parseInt(document.getElementById('y-mode').value) || 0,
      yLeft:  parseFloat(document.getElementById('y-left').value) || 0,
      yRight: parseFloat(document.getElementById('y-right').value) || 0,
      bMode:  parseInt(document.getElementById('b-mode').value) || 0,
      bLeft:  parseFloat(document.getElementById('b-left').value) || 0,
      bRight: parseFloat(document.getElementById('b-right').value) || 0,
      aMode:  parseInt(document.getElementById('a-mode').value) || 0,
      aLeft:  parseFloat(document.getElementById('a-left').value) || 0,
      aRight: parseFloat(document.getElementById('a-right').value) || 0
    };

    var btn = document.getElementById('save-presets-btn');
    btn.disabled = true;

    fetch('/settingsdata', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) {
        showStatus(presetsStatus, 'Button actions saved!', true);
      } else {
        showStatus(presetsStatus, 'Save failed', false);
      }
      btn.disabled = false;
    })
    .catch(function(err) {
      showStatus(presetsStatus, 'Save failed: ' + err, false);
      btn.disabled = false;
    });
  };

  // ---- Save motor tuning (speed + acceleration + current) ----
  window.saveMotorTuning = function() {
    var spd = parseFloat(document.getElementById('speed-limit').value);
    var accel = parseFloat(document.getElementById('acceleration').value);
    var cur = parseFloat(document.getElementById('current-limit').value);

    if (isNaN(spd) || spd < 0.1 || spd > 50.0) {
      showStatus(speedStatus, 'Speed must be between 0.1 and 50.0 rad/s', false);
      return;
    }
    if (isNaN(accel) || accel < 1 || accel > 500) {
      showStatus(speedStatus, 'Acceleration must be between 1 and 500 rad/s^2', false);
      return;
    }
    if (isNaN(cur) || cur < 0.5 || cur > 40) {
      showStatus(speedStatus, 'Current must be between 0.5 and 40.0 A', false);
      return;
    }

    var btn = document.getElementById('save-motor-btn');
    btn.disabled = true;

    fetch('/settingsdata', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ speedLimit: spd, acceleration: accel, currentLimit: cur })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) {
        showStatus(speedStatus, 'Motor tuning saved! (spd=' + spd + ', accel=' + accel + ', cur=' + cur + ')', true);
      } else {
        showStatus(speedStatus, 'Save failed', false);
      }
      btn.disabled = false;
    })
    .catch(function(err) {
      showStatus(speedStatus, 'Save failed: ' + err, false);
      btn.disabled = false;
    });
  };

  // ---- Save motor config (dropdown) ----
  window.saveConfig = function() {
    var leftId = parseInt(leftSelect.value) || 0;
    var rightId = parseInt(rightSelect.value) || 0;

    if (leftId > 0 && leftId === rightId) {
      showStatus(configStatus, 'Left and right cannot be the same motor!', false);
      return;
    }

    var btn = document.getElementById('save-btn');
    btn.disabled = true;

    fetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ leftId: leftId, rightId: rightId })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) {
        showStatus(configStatus, 'Configuration saved! Left=' + d.leftId + ', Right=' + d.rightId, true);
        manualLeft.value = d.leftId;
        manualRight.value = d.rightId;
      } else {
        showStatus(configStatus, 'Save failed', false);
      }
      btn.disabled = false;
    })
    .catch(function(err) {
      showStatus(configStatus, 'Save failed: ' + err, false);
      btn.disabled = false;
    });
  };

  // ---- Save manual motor IDs ----
  window.saveManual = function() {
    var leftId = parseInt(manualLeft.value) || 0;
    var rightId = parseInt(manualRight.value) || 0;

    if (leftId > 0 && leftId === rightId) {
      showStatus(configStatus, 'Left and right cannot be the same motor!', false);
      return;
    }
    if (leftId < 0 || leftId > 127 || rightId < 0 || rightId > 127) {
      showStatus(configStatus, 'Motor IDs must be 0-127 (0 = none)', false);
      return;
    }

    fetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ leftId: leftId, rightId: rightId })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) {
        showStatus(configStatus, 'Manual IDs saved! Left=' + d.leftId + ', Right=' + d.rightId, true);
        leftSelect.value = d.leftId;
        rightSelect.value = d.rightId;
      } else {
        showStatus(configStatus, 'Save failed', false);
      }
    })
    .catch(function(err) {
      showStatus(configStatus, 'Save failed: ' + err, false);
    });
  };

  // ---- Load everything on page open ----
  loadMotorConfig();
  loadSettings();
})();
</script>
</body>
</html>
)rawliteral";
