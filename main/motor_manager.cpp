// =============================================================================
// Motor Manager Module - Implementation
// =============================================================================
// CAN bus communication with Robstride motors via ESP32 TWAI.
// Protocol cross-referenced from JumpRopeM5, sirwart/robstride Python SDK,
// and RobStride/robstride_ros_sample.
// =============================================================================

#include "motor_manager.h"
#include "config.h"
#include "debug_log.h"

#include <driver/twai.h>
#include <Preferences.h>
#include <cstring>

static const char* TAG = "Motor";

// Zeroed status returned for out-of-range index queries
static const RobstrideMotorStatus EMPTY_STATUS = {};

// =============================================================================
// Public Methods
// =============================================================================

void MotorManager::begin() {
    LOG_INFO(TAG, "Initializing CAN bus motor manager...");
    LOG_INFO(TAG, "  TX pin: GPIO%d, RX pin: GPIO%d", PIN_CAN_TX, PIN_CAN_RX);
    LOG_INFO(TAG, "  Baud rate: %d, Master ID: 0x%02X", CAN_BAUD_RATE, _masterId);

    // Load motor role config from NVS
    loadConfig();

    _motorCount = 0;
    memset(_motorIds, 0, sizeof(_motorIds));
    memset(_motorStatus, 0, sizeof(_motorStatus));

    if (!initTwai()) {
        LOG_ERROR(TAG, "TWAI initialization failed! Motors will not be available.");
        return;
    }

    LOG_INFO(TAG, "TWAI driver started. Scanning for motors...");
    scanMotors();

    if (_motorCount > 0) {
        LOG_INFO(TAG, "Found %d motor(s). Reading initial voltage...", _motorCount);
        // Kick off first VBUS read
        _lastVbusPollMs = 0;
    } else {
        LOG_WARN(TAG, "No motors found on CAN bus. Check wiring and power.");
    }
}

void MotorManager::poll() {
    if (!_running) {
        return;
    }

    // Process all pending CAN messages (non-blocking)
    twai_message_t msg;
    int count = 0;
    while (twai_receive(&msg, 0) == ESP_OK) {
        if (msg.extd) {
            processMessage(msg.identifier, msg.data, msg.data_length_code);
        }
        count++;
        // Safety limit to avoid blocking the loop too long
        if (count >= 50) {
            break;
        }
    }

    // Check for parameter read timeout
    if (_paramReadPending) {
        unsigned long now = millis();
        if (now - _paramReadStartMs > 200) {
            LOG_DEBUG(TAG, "Parameter read timeout for motor %d, param 0x%04X",
                      _paramReadMotorId, _paramReadIndex);
            _paramReadPending = false;
        }
    }

    // Periodic status pings to keep position/velocity/torque live
    pollStatus();

    // Check for stale or disconnected motors
    checkStaleness();

    // Periodic voltage polling
    pollVoltage();
}

void MotorManager::rescan() {
    if (!_running) {
        return;
    }
    LOG_INFO(TAG, "Re-scanning CAN bus for motors...");
    _motorCount = 0;
    memset(_motorIds, 0, sizeof(_motorIds));
    memset(_motorStatus, 0, sizeof(_motorStatus));
    scanMotors();
}

int MotorManager::getMotorCount() const {
    return _motorCount;
}

const RobstrideMotorStatus& MotorManager::getMotorStatus(int index) const {
    if (index >= 0 && index < _motorCount) {
        return _motorStatus[index];
    }
    return EMPTY_STATUS;
}

uint8_t MotorManager::getMotorId(int index) const {
    if (index >= 0 && index < _motorCount) {
        return _motorIds[index];
    }
    return 0;
}

bool MotorManager::isRunning() const {
    return _running;
}

// =============================================================================
// Motor Role Configuration (NVS-persisted)
// =============================================================================

uint8_t MotorManager::getLeftMotorId() const {
    return _leftMotorId;
}

void MotorManager::setLeftMotorId(uint8_t id) {
    _leftMotorId = id;
    saveConfig();
    LOG_INFO(TAG, "Left motor set to CAN ID: %d", id);
}

uint8_t MotorManager::getRightMotorId() const {
    return _rightMotorId;
}

void MotorManager::setRightMotorId(uint8_t id) {
    _rightMotorId = id;
    saveConfig();
    LOG_INFO(TAG, "Right motor set to CAN ID: %d", id);
}

const char* MotorManager::getRoleLabel(uint8_t motorId) const {
    if (motorId > 0 && motorId == _leftMotorId) {
        return "L";
    }
    if (motorId > 0 && motorId == _rightMotorId) {
        return "R";
    }
    return "";
}

const RobstrideMotorStatus& MotorManager::getLeftMotorStatus() const {
    if (_leftMotorId > 0) {
        for (int i = 0; i < _motorCount; i++) {
            if (_motorIds[i] == _leftMotorId) {
                return _motorStatus[i];
            }
        }
    }
    return EMPTY_STATUS;
}

const RobstrideMotorStatus& MotorManager::getRightMotorStatus() const {
    if (_rightMotorId > 0) {
        for (int i = 0; i < _motorCount; i++) {
            if (_motorIds[i] == _rightMotorId) {
                return _motorStatus[i];
            }
        }
    }
    return EMPTY_STATUS;
}

void MotorManager::loadConfig() {
    Preferences prefs;
    prefs.begin("motors", true);  // read-only
    _leftMotorId = prefs.getUChar("leftId", 0);
    _rightMotorId = prefs.getUChar("rightId", 0);
    prefs.end();
    LOG_INFO(TAG, "Loaded motor config: left=%d, right=%d", _leftMotorId, _rightMotorId);
}

void MotorManager::saveConfig() {
    Preferences prefs;
    prefs.begin("motors", false);  // read-write
    prefs.putUChar("leftId", _leftMotorId);
    prefs.putUChar("rightId", _rightMotorId);
    prefs.end();
    LOG_INFO(TAG, "Saved motor config: left=%d, right=%d", _leftMotorId, _rightMotorId);
}

// =============================================================================
// Motor Commands
// =============================================================================

bool MotorManager::enableMotor(uint8_t motorId) {
    if (!_running) {
        return false;
    }

    uint8_t data[8] = {0};
    uint32_t id = buildExtendedId(RobstrideComm::MOTOR_ENABLE, motorId);
    bool ok = sendMessage(id, data, 8);
    if (ok) {
        LOG_INFO(TAG, "Enabled motor %d", motorId);
    }
    return ok;
}

bool MotorManager::stopMotor(uint8_t motorId, bool clearFaults) {
    if (!_running) {
        return false;
    }

    uint8_t data[8] = {0};
    if (clearFaults) {
        data[0] = 1;
    }
    uint32_t id = buildExtendedId(RobstrideComm::MOTOR_STOP, motorId);
    bool ok = sendMessage(id, data, 8);
    if (ok) {
        LOG_INFO(TAG, "Stopped motor %d (clearFaults=%d)", motorId, clearFaults);
    }
    return ok;
}

bool MotorManager::setMechanicalZero(uint8_t motorId) {
    if (!_running) {
        return false;
    }

    uint8_t data[8] = {0};
    data[0] = 1;  // Required: data[0]=1 to confirm zero set
    uint32_t id = buildExtendedId(RobstrideComm::SET_MECHANICAL_ZERO, motorId);
    bool ok = sendMessage(id, data, 8);
    if (ok) {
        LOG_INFO(TAG, "Set mechanical zero for motor %d", motorId);
    }
    return ok;
}

bool MotorManager::writeFloatParam(uint8_t motorId, uint16_t paramIndex, float value) {
    if (!_running) {
        return false;
    }

    uint8_t data[8] = {0};
    // data[0-1]: parameter index (little-endian)
    data[0] = paramIndex & 0xFF;
    data[1] = (paramIndex >> 8) & 0xFF;
    // data[4-7]: float value (little-endian IEEE 754)
    memcpy(&data[4], &value, sizeof(float));

    uint32_t id = buildExtendedId(RobstrideComm::SET_SINGLE_PARAM, motorId);
    bool ok = sendMessage(id, data, 8);
    if (ok) {
        LOG_DEBUG(TAG, "Wrote float param 0x%04X = %.4f to motor %d",
                  paramIndex, value, motorId);
    }
    return ok;
}

bool MotorManager::writeUint8Param(uint8_t motorId, uint16_t paramIndex, uint8_t value) {
    if (!_running) {
        return false;
    }

    uint8_t data[8] = {0};
    // data[0-1]: parameter index (little-endian)
    data[0] = paramIndex & 0xFF;
    data[1] = (paramIndex >> 8) & 0xFF;
    // data[4]: uint8 value
    data[4] = value;

    uint32_t id = buildExtendedId(RobstrideComm::SET_SINGLE_PARAM, motorId);
    bool ok = sendMessage(id, data, 8);
    if (ok) {
        LOG_DEBUG(TAG, "Wrote uint8 param 0x%04X = %d to motor %d",
                  paramIndex, value, motorId);
    }
    return ok;
}

// =============================================================================
// TWAI Initialization
// =============================================================================

bool MotorManager::initTwai() {
    if (_running) {
        // Already running -- stop first
        twai_stop();
        twai_driver_uninstall();
        _running = false;
    }

    // Timing configuration for 1 Mbps
    // ESP32 with 40MHz crystal: quanta_resolution = 20MHz, tseg1=15, tseg2=4
    // (Equivalent to TWAI_TIMING_CONFIG_1MBITS() which may not be available
    //  in all build configs due to CONFIG_XTAL_FREQ guards)
    twai_timing_config_t timingConfig = {};
    timingConfig.clk_src = TWAI_CLK_SRC_DEFAULT;
    timingConfig.quanta_resolution_hz = 20000000;
    timingConfig.brp = 0;
    timingConfig.tseg_1 = 15;
    timingConfig.tseg_2 = 4;
    timingConfig.sjw = 3;
    timingConfig.triple_sampling = false;

    // General configuration
    twai_general_config_t generalConfig = {};
    generalConfig.controller_id = 0;
    generalConfig.mode = TWAI_MODE_NORMAL;
    generalConfig.tx_io = (gpio_num_t)PIN_CAN_TX;
    generalConfig.rx_io = (gpio_num_t)PIN_CAN_RX;
    generalConfig.clkout_io = TWAI_IO_UNUSED;
    generalConfig.bus_off_io = TWAI_IO_UNUSED;
    generalConfig.tx_queue_len = 16;
    generalConfig.rx_queue_len = 32;
    generalConfig.alerts_enabled = TWAI_ALERT_NONE;
    generalConfig.clkout_divider = 0;
    generalConfig.intr_flags = 0;

    // Accept all messages (we filter in software)
    twai_filter_config_t filterConfig = {};
    filterConfig.acceptance_code = 0;
    filterConfig.acceptance_mask = 0xFFFFFFFF;
    filterConfig.single_filter = true;

    // Install driver
    esp_err_t result = twai_driver_install(&generalConfig, &timingConfig, &filterConfig);
    if (result != ESP_OK) {
        LOG_ERROR(TAG, "TWAI driver install failed: %s (0x%x)",
                  esp_err_to_name(result), result);
        return false;
    }

    // Start driver
    result = twai_start();
    if (result != ESP_OK) {
        LOG_ERROR(TAG, "TWAI start failed: %s (0x%x)", esp_err_to_name(result), result);
        twai_driver_uninstall();
        return false;
    }

    _running = true;
    LOG_INFO(TAG, "TWAI driver initialized successfully");
    return true;
}

// =============================================================================
// Motor Discovery
// =============================================================================

void MotorManager::scanMotors() {
    if (!_running) {
        return;
    }

    // Drain any pending messages first
    twai_message_t drainMsg;
    while (twai_receive(&drainMsg, 0) == ESP_OK) {
        // discard
    }

    uint8_t data[8] = {0};

    // 1) Send broadcast GET_ID to address 0x7F
    LOG_INFO(TAG, "Sending broadcast GET_ID...");
    uint32_t broadcastId = buildExtendedId(RobstrideComm::GET_ID, 0x7F);
    sendMessage(broadcastId, data, 8);
    delay(20);

    // 2) Send individual GET_ID probes in batches (1-127)
    //    Batching avoids TX queue overflow.
    LOG_INFO(TAG, "Probing motor IDs 1-127...");
    for (int batch = 0; batch < 4; batch++) {
        int startId = batch * 32 + 1;
        int endId = (batch + 1) * 32;
        if (endId > 127) {
            endId = 127;
        }

        for (int i = startId; i <= endId; i++) {
            // Skip our own master ID
            if ((uint8_t)i == _masterId) {
                continue;
            }

            uint32_t probeId = buildExtendedId(RobstrideComm::GET_ID, (uint8_t)i);
            if (!sendMessage(probeId, data, 8)) {
                // TX queue probably full, wait and drain
                delay(10);
                twai_message_t rxMsg;
                while (twai_receive(&rxMsg, 0) == ESP_OK) {
                    if (rxMsg.extd) {
                        processMessage(rxMsg.identifier, rxMsg.data, rxMsg.data_length_code);
                    }
                }
                // Retry
                sendMessage(probeId, data, 8);
            }

            // Drain RX every 8 probes to keep up
            if (i % 8 == 0) {
                delay(2);
                twai_message_t rxMsg;
                while (twai_receive(&rxMsg, 0) == ESP_OK) {
                    if (rxMsg.extd) {
                        processMessage(rxMsg.identifier, rxMsg.data, rxMsg.data_length_code);
                    }
                }
            }
        }

        // Drain between batches
        delay(20);
        twai_message_t rxMsg;
        while (twai_receive(&rxMsg, 0) == ESP_OK) {
            if (rxMsg.extd) {
                processMessage(rxMsg.identifier, rxMsg.data, rxMsg.data_length_code);
            }
        }
    }

    // 3) Wait for remaining responses
    LOG_INFO(TAG, "Waiting for scan responses...");
    unsigned long scanStart = millis();
    while (millis() - scanStart < CAN_SCAN_TIMEOUT_MS) {
        twai_message_t rxMsg;
        esp_err_t result = twai_receive(&rxMsg, pdMS_TO_TICKS(10));
        if (result == ESP_OK && rxMsg.extd) {
            uint32_t canId = rxMsg.identifier;
            uint8_t commType = (canId >> 24) & 0x1F;
            uint8_t motorId = (canId >> 8) & 0xFF;

            // Accept GET_ID or MOTOR_FEEDBACK responses as discovery
            if (commType == RobstrideComm::GET_ID || commType == RobstrideComm::MOTOR_FEEDBACK) {
                if (motorId != _masterId && motorId > 0 && motorId <= 127) {
                    int idx = addMotor(motorId);
                    if (idx >= 0) {
                        LOG_INFO(TAG, "Discovered motor ID: %d", motorId);
                    }
                }
            }

            // Also process feedback if present
            processMessage(canId, rxMsg.data, rxMsg.data_length_code);
        }
    }

    LOG_INFO(TAG, "Scan complete. Found %d motor(s):", _motorCount);
    for (int i = 0; i < _motorCount; i++) {
        LOG_INFO(TAG, "  Motor[%d]: CAN ID = %d", i, _motorIds[i]);
    }
}

// =============================================================================
// Motor Index Management
// =============================================================================

int MotorManager::findMotorIndex(uint8_t motorId) const {
    for (int i = 0; i < _motorCount; i++) {
        if (_motorIds[i] == motorId) {
            return i;
        }
    }
    return -1;
}

int MotorManager::addMotor(uint8_t motorId) {
    // Check if already known
    int existing = findMotorIndex(motorId);
    if (existing >= 0) {
        return existing;
    }

    // Check capacity
    if (_motorCount >= MAX_MOTORS) {
        LOG_WARN(TAG, "Motor list full (%d), cannot add ID %d", MAX_MOTORS, motorId);
        return -1;
    }

    int idx = _motorCount;
    _motorIds[idx] = motorId;
    memset(&_motorStatus[idx], 0, sizeof(RobstrideMotorStatus));
    _motorStatus[idx].lastUpdateMs = millis();  // Mark discovery time for staleness tracking
    _motorCount++;
    return idx;
}

// =============================================================================
// CAN Message Building & Sending
// =============================================================================

uint32_t MotorManager::buildExtendedId(uint8_t commType, uint8_t motorId, uint16_t extraData) {
    // For MOTION_CONTROL, torque uint16 goes in bits 8-23
    if (commType == RobstrideComm::MOTION_CONTROL) {
        return ((uint32_t)commType << 24) | ((uint32_t)extraData << 8) | motorId;
    }

    // Standard format: commType | masterId | motorId
    return ((uint32_t)commType << 24) | ((uint32_t)_masterId << 8) | motorId;
}

bool MotorManager::sendMessage(uint32_t id, const uint8_t* data, uint8_t len) {
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier = id;
    msg.extd = 1;
    msg.data_length_code = len;

    if (data && len > 0) {
        memcpy(msg.data, data, len);
    }

    esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(50));
    if (result != ESP_OK) {
        LOG_DEBUG(TAG, "CAN TX failed (ID: 0x%08X, err: %s)", id, esp_err_to_name(result));
        return false;
    }
    return true;
}

// =============================================================================
// Message Processing
// =============================================================================

void MotorManager::processMessage(uint32_t canId, const uint8_t* data, uint8_t len) {
    uint8_t commType = (canId >> 24) & 0x1F;
    uint8_t motorId = (canId >> 8) & 0xFF;

    switch (commType) {
        case RobstrideComm::MOTOR_FEEDBACK:
            parseMotorFeedback(motorId, canId, data);
            break;

        case RobstrideComm::GET_SINGLE_PARAM:
        case RobstrideComm::SET_SINGLE_PARAM: {
            // Parameter responses are addressed to us (master)
            uint8_t destId = canId & 0xFF;
            if (destId == _masterId) {
                parseParameterResponse(motorId, data);
            }
            break;
        }

        case RobstrideComm::GET_ID:
            // Motor identification response -- add if not known
            if (motorId != _masterId && motorId > 0 && motorId <= 127) {
                int idx = addMotor(motorId);
                // Update lastUpdateMs for already-known motors too (keeps alive for staleness check)
                if (idx >= 0) {
                    _motorStatus[idx].lastUpdateMs = millis();
                    _motorStatus[idx].stale = false;
                }
            }
            break;

        default:
            break;
    }
}

void MotorManager::parseMotorFeedback(uint8_t motorId, uint32_t canId, const uint8_t* data) {
    int idx = findMotorIndex(motorId);
    if (idx < 0) {
        // Unknown motor -- try to add it
        idx = addMotor(motorId);
        if (idx < 0) {
            return;
        }
    }

    RobstrideMotorStatus& status = _motorStatus[idx];

    // Use RS02 spec as default (our motors are RS-02)
    const RobstrideMotorSpec& spec = ROBSTRIDE_DEFAULT_SPEC;

    // Parse status bits from CAN ID
    // Bits 16-21: error code (6 bits)
    // Bits 22-23: pattern/mode (0=reset, 1=calibration, 2=running)
    uint8_t errorCode = (canId >> 16) & 0x3F;
    uint8_t pattern = (canId >> 22) & 0x03;

    // Parse data bytes (big-endian uint16 pairs)
    uint16_t posU16 = ((uint16_t)data[0] << 8) | data[1];
    uint16_t velU16 = ((uint16_t)data[2] << 8) | data[3];
    uint16_t torU16 = ((uint16_t)data[4] << 8) | data[5];
    uint16_t tempU16 = ((uint16_t)data[6] << 8) | data[7];

    // Convert to physical values
    status.position    = uintToFloat(posU16, -spec.positionLimit, spec.positionLimit, 16);
    status.velocity    = uintToFloat(velU16, -spec.velocityLimit, spec.velocityLimit, 16);
    status.torque      = uintToFloat(torU16, -spec.torqueLimit, spec.torqueLimit, 16);
    status.temperature = tempU16 * 0.1f;

    status.errorCode = errorCode;
    status.mode = pattern;
    status.hasFault = (errorCode != 0);
    status.enabled = (pattern == RobstrideState::RUNNING);
    status.lastUpdateMs = millis();
}

void MotorManager::parseParameterResponse(uint8_t motorId, const uint8_t* data) {
    // Parameter index is in data[0-1] (little-endian)
    uint16_t paramIndex = (uint16_t)data[0] | ((uint16_t)data[1] << 8);

    // Check if this is the parameter we're waiting for
    if (_paramReadPending && paramIndex == _paramReadIndex && motorId == _paramReadMotorId) {
        // Float value in data[4-7] (little-endian IEEE 754)
        memcpy(&_paramReadValue, &data[4], sizeof(float));
        _paramReadPending = false;

        // If this was a VBUS read, store it in the motor status
        if (paramIndex == RobstrideParam::VBUS) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].voltage = _paramReadValue;
                LOG_DEBUG(TAG, "Motor %d VBUS: %.1fV", motorId, _paramReadValue);
            }
        }

        // If this was a RUN_MODE read, store it
        if (paramIndex == RobstrideParam::RUN_MODE) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].runMode = (uint8_t)data[4];
            }
        }

        // If this was a PP_SPEED read, store it
        if (paramIndex == RobstrideParam::PP_SPEED) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].ppSpeed = _paramReadValue;
                LOG_INFO(TAG, "Motor %d PP_SPEED readback: %.2f rad/s", motorId, _paramReadValue);
            }
        }

        // If this was a PP_ACCELERATION read, store it
        if (paramIndex == RobstrideParam::PP_ACCELERATION) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].ppAccel = _paramReadValue;
                LOG_INFO(TAG, "Motor %d PP_ACCEL readback: %.2f rad/s^2", motorId, _paramReadValue);
            }
        }

        // If this was a LIMIT_SPD read, store it
        if (paramIndex == RobstrideParam::LIMIT_SPD) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].limitSpd = _paramReadValue;
                LOG_INFO(TAG, "Motor %d LIMIT_SPD readback: %.2f rad/s", motorId, _paramReadValue);
            }
        }

        // If this was a LIMIT_CUR read, store it
        if (paramIndex == RobstrideParam::LIMIT_CUR) {
            int idx = findMotorIndex(motorId);
            if (idx >= 0) {
                _motorStatus[idx].limitCur = _paramReadValue;
                LOG_INFO(TAG, "Motor %d LIMIT_CUR readback: %.2f A", motorId, _paramReadValue);
            }
        }
    }
}

// =============================================================================
// Parameter Reading
// =============================================================================

bool MotorManager::requestParameter(uint8_t motorId, uint16_t paramIndex) {
    if (_paramReadPending) {
        return false;  // Already waiting for a response
    }

    uint8_t data[8] = {0};
    data[0] = paramIndex & 0xFF;
    data[1] = (paramIndex >> 8) & 0xFF;

    uint32_t id = buildExtendedId(RobstrideComm::GET_SINGLE_PARAM, motorId);
    if (!sendMessage(id, data, 8)) {
        return false;
    }

    _paramReadPending = true;
    _paramReadIndex = paramIndex;
    _paramReadMotorId = motorId;
    _paramReadValue = 0;
    _paramReadStartMs = millis();
    return true;
}

// =============================================================================
// Periodic Status Polling
// =============================================================================

void MotorManager::pollStatus() {
    if (_motorCount == 0) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastStatusPollMs < MOTOR_STATUS_POLL_MS) {
        return;
    }
    _lastStatusPollMs = now;

    // Round-robin: ping one motor per interval
    if (_statusPollMotorIndex >= _motorCount) {
        _statusPollMotorIndex = 0;
    }

    uint8_t motorId = _motorIds[_statusPollMotorIndex];
    _statusPollMotorIndex++;

    // Send a zero-torque MOTION_CONTROL frame (type 0x01).
    // This is the standard way to request status from Robstride motors
    // (same as sirwart/robstride Python SDK's get_motor_status).
    // The motor responds with MOTOR_FEEDBACK (type 0x02) containing
    // live position, velocity, torque, and temperature.
    // Data bytes are all zero (zero position, zero velocity, zero kp, zero kd).
    // Torque is encoded in the CAN ID extra bits, also zero.
    uint8_t data[8] = {0};
    // buildExtendedId for MOTION_CONTROL puts extraData (torque uint16) in bits 8-23
    // With extraData=0, this is a zero-torque, zero-everything command.
    uint32_t id = ((uint32_t)RobstrideComm::MOTION_CONTROL << 24) | ((uint32_t)0x0000 << 8) | motorId;
    sendMessage(id, data, 8);
}

// =============================================================================
// Stale / Disconnect Detection
// =============================================================================

void MotorManager::checkStaleness() {
    if (_motorCount == 0) {
        return;
    }

    unsigned long now = millis();

    // Walk backwards so removal doesn't skip entries
    for (int i = _motorCount - 1; i >= 0; i--) {
        unsigned long lastUpdate = _motorStatus[i].lastUpdateMs;

        // If lastUpdateMs is 0, the motor was just added and hasn't responded yet.
        // Give it the scan timeout before considering it stale.
        if (lastUpdate == 0) {
            continue;
        }

        unsigned long elapsed = now - lastUpdate;

        // Remove motor entirely after MOTOR_REMOVE_MS of silence
        if (elapsed >= MOTOR_REMOVE_MS) {
            LOG_WARN(TAG, "Motor %d removed (no response for %lus)",
                     _motorIds[i], elapsed / 1000);
            removeMotor(i);
            continue;
        }

        // Mark as stale after MOTOR_STALE_MS of silence
        if (elapsed >= MOTOR_STALE_MS) {
            if (!_motorStatus[i].stale) {
                LOG_WARN(TAG, "Motor %d stale (no response for %lums)",
                         _motorIds[i], elapsed);
                _motorStatus[i].stale = true;
            }
        } else {
            _motorStatus[i].stale = false;
        }
    }
}

void MotorManager::removeMotor(int index) {
    if (index < 0 || index >= _motorCount) {
        return;
    }

    // Shift remaining motors down to fill the gap
    for (int i = index; i < _motorCount - 1; i++) {
        _motorIds[i] = _motorIds[i + 1];
        _motorStatus[i] = _motorStatus[i + 1];
    }
    _motorCount--;

    // Reset the last slot
    _motorIds[_motorCount] = 0;
    memset(&_motorStatus[_motorCount], 0, sizeof(RobstrideMotorStatus));

    // Fix round-robin indices if they pointed past the removed entry
    if (_statusPollMotorIndex > _motorCount) {
        _statusPollMotorIndex = 0;
    }
    if (_vbusPollMotorIndex > _motorCount) {
        _vbusPollMotorIndex = 0;
    }
}

// =============================================================================
// Periodic Voltage Polling
// =============================================================================

void MotorManager::pollVoltage() {
    if (_motorCount == 0) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastVbusPollMs < MOTOR_VBUS_POLL_MS) {
        return;
    }

    // Don't start a new read if one is pending
    if (_paramReadPending) {
        return;
    }

    _lastVbusPollMs = now;

    // Round-robin through motors
    if (_vbusPollMotorIndex >= _motorCount) {
        _vbusPollMotorIndex = 0;
    }

    uint8_t motorId = _motorIds[_vbusPollMotorIndex];
    requestParameter(motorId, RobstrideParam::VBUS);
    _vbusPollMotorIndex++;
}

// =============================================================================
// Public Parameter Read API
// =============================================================================

bool MotorManager::requestParamRead(uint8_t motorId, uint16_t paramIndex) {
    return requestParameter(motorId, paramIndex);
}

bool MotorManager::isParamReadPending() const {
    return _paramReadPending;
}

float MotorManager::getLastParamReadValue() const {
    return _paramReadValue;
}

// =============================================================================
// Conversion Helpers
// =============================================================================

float MotorManager::uintToFloat(uint16_t x, float xMin, float xMax, int bits) {
    uint32_t maxVal = (1 << bits) - 1;
    float span = xMax - xMin;
    return ((float)x / (float)maxVal) * span + xMin;
}

uint16_t MotorManager::floatToUint(float x, float xMin, float xMax, int bits) {
    if (x < xMin) x = xMin;
    if (x > xMax) x = xMax;
    float span = xMax - xMin;
    float offset = x - xMin;
    uint32_t maxVal = (1 << bits) - 1;
    return (uint16_t)((offset / span) * (float)maxVal);
}
