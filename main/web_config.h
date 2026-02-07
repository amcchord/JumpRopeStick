#pragma once

// =============================================================================
// Embedded Web Config Page - Motor role assignment
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
  .form-row {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-bottom: 12px;
  }
  .form-row label {
    width: 100px;
    font-size: 0.9em;
    color: #999;
    flex-shrink: 0;
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
  .motor-id-hint {
    font-size: 0.75em;
    color: #555;
    margin-top: 2px;
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
</style>
</head>
<body>
  <a href="/" class="back-link">&larr; Back to Dashboard</a>
  <h1>Settings</h1>

  <div class="card">
    <h2>Motor Role Assignment</h2>
    <p style="font-size:0.85em;color:#777;margin-bottom:14px;">
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

    <div class="status-msg" id="status-msg"></div>
  </div>

  <div class="card">
    <h2>Manual Motor ID Entry</h2>
    <p style="font-size:0.85em;color:#777;margin-bottom:14px;">
      If a motor is not currently connected, you can type its CAN ID directly (1-127).
    </p>
    <div class="form-row">
      <label for="manual-left">Left ID</label>
      <input type="number" id="manual-left" min="0" max="127" value="0"
        style="flex:1;background:#12141c;color:#fff;border:1px solid #333;border-radius:6px;padding:8px 12px;font-size:0.9em;font-family:'SF Mono',monospace;">
    </div>
    <div class="form-row">
      <label for="manual-right">Right ID</label>
      <input type="number" id="manual-right" min="0" max="127" value="0"
        style="flex:1;background:#12141c;color:#fff;border:1px solid #333;border-radius:6px;padding:8px 12px;font-size:0.9em;font-family:'SF Mono',monospace;">
    </div>
    <div class="btn-row">
      <button class="btn" onclick="saveManual()">Save Manual IDs</button>
    </div>
  </div>

<script>
(function() {
  var leftSelect = document.getElementById('left-motor');
  var rightSelect = document.getElementById('right-motor');
  var manualLeft = document.getElementById('manual-left');
  var manualRight = document.getElementById('manual-right');
  var statusMsg = document.getElementById('status-msg');
  var discoveredIds = document.getElementById('discovered-ids');

  function loadConfig() {
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
        showStatus('Failed to load config: ' + err, false);
      });
  }

  function populateDropdowns(discovered, leftId, rightId) {
    // Build option lists
    var noneOpt = '<option value="0">-- None --</option>';

    var leftHtml = noneOpt;
    var rightHtml = noneOpt;

    // Add discovered motors
    for (var i = 0; i < discovered.length; i++) {
      var id = discovered[i];
      leftHtml += '<option value="' + id + '">Motor ' + id + '</option>';
      rightHtml += '<option value="' + id + '">Motor ' + id + '</option>';
    }

    // If the saved ID is not in the discovered list, add it as an option
    if (leftId > 0) {
      var leftFound = false;
      for (var j = 0; j < discovered.length; j++) {
        if (discovered[j] === leftId) {
          leftFound = true;
          break;
        }
      }
      if (!leftFound) {
        leftHtml += '<option value="' + leftId + '">Motor ' + leftId + ' (offline)</option>';
      }
    }
    if (rightId > 0) {
      var rightFound = false;
      for (var k = 0; k < discovered.length; k++) {
        if (discovered[k] === rightId) {
          rightFound = true;
          break;
        }
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

  function showStatus(msg, success) {
    statusMsg.textContent = msg;
    statusMsg.className = 'status-msg ' + (success ? 'ok' : 'err');
    if (success) {
      setTimeout(function() {
        statusMsg.className = 'status-msg';
      }, 3000);
    }
  }

  window.saveConfig = function() {
    var leftId = parseInt(leftSelect.value) || 0;
    var rightId = parseInt(rightSelect.value) || 0;

    // Validate: can't assign the same motor to both sides
    if (leftId > 0 && leftId === rightId) {
      showStatus('Left and right cannot be the same motor!', false);
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
        showStatus('Configuration saved! Left=' + d.leftId + ', Right=' + d.rightId, true);
        manualLeft.value = d.leftId;
        manualRight.value = d.rightId;
      } else {
        showStatus('Save failed', false);
      }
      btn.disabled = false;
    })
    .catch(function(err) {
      showStatus('Save failed: ' + err, false);
      btn.disabled = false;
    });
  };

  window.saveManual = function() {
    var leftId = parseInt(manualLeft.value) || 0;
    var rightId = parseInt(manualRight.value) || 0;

    if (leftId > 0 && leftId === rightId) {
      showStatus('Left and right cannot be the same motor!', false);
      return;
    }
    if (leftId < 0 || leftId > 127 || rightId < 0 || rightId > 127) {
      showStatus('Motor IDs must be 0-127 (0 = none)', false);
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
        showStatus('Manual IDs saved! Left=' + d.leftId + ', Right=' + d.rightId, true);
        leftSelect.value = d.leftId;
        rightSelect.value = d.rightId;
      } else {
        showStatus('Save failed', false);
      }
    })
    .catch(function(err) {
      showStatus('Save failed: ' + err, false);
    });
  };

  // Load on page open
  loadConfig();
})();
</script>
</body>
</html>
)rawliteral";
