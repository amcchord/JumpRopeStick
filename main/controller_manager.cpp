// =============================================================================
// Controller Manager Module - Implementation
// =============================================================================

#include "controller_manager.h"
#include "debug_log.h"
#include <Bluepad32.h>

static const char* TAG = "Controller";

// Bluepad32 raw controller pointers (used in callbacks)
static ControllerPtr s_rawControllers[BP32_MAX_GAMEPADS] = {nullptr};

// Processed state for each controller slot
static ControllerState s_states[CONTROLLER_MAX_COUNT];

// Global instance
ControllerManager g_controllerManager;

// ---------------------------------------------------------------------------
// Static Bluepad32 callbacks
// ---------------------------------------------------------------------------

void ControllerManager::onConnected(ControllerPtr ctl) {
    bool foundSlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_rawControllers[i] == nullptr) {
            s_rawControllers[i] = ctl;
            foundSlot = true;

            ControllerProperties props = ctl->getProperties();
            LOG_INFO(TAG, "Controller connected at slot %d", i);
            String modelStr = ctl->getModelName();
            LOG_INFO(TAG, "  Model: %s", modelStr.c_str());
            LOG_INFO(TAG, "  VID: 0x%04x  PID: 0x%04x", props.vendor_id, props.product_id);

            // Store model name in state
            strncpy(s_states[i].modelName, modelStr.c_str(), sizeof(s_states[i].modelName) - 1);
            s_states[i].modelName[sizeof(s_states[i].modelName) - 1] = '\0';
            break;
        }
    }

    if (!foundSlot) {
        LOG_WARN(TAG, "Controller connected but no empty slot available");
    }
}

void ControllerManager::onDisconnected(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_rawControllers[i] == ctl) {
            LOG_INFO(TAG, "Controller disconnected from slot %d", i);
            s_rawControllers[i] = nullptr;

            // Clear the state for this slot
            memset(&s_states[i], 0, sizeof(ControllerState));
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void ControllerManager::begin() {
    LOG_INFO(TAG, "Initializing Bluepad32...");

    // Clear all states
    memset(s_states, 0, sizeof(s_states));

    // Setup Bluepad32 with our callbacks
    BP32.setup(&ControllerManager::onConnected, &ControllerManager::onDisconnected, true);

    // Don't forget Bluetooth keys on every boot (allow re-pairing)
    // Uncomment the next line if you want to clear paired devices on every boot:
    // BP32.forgetBluetoothKeys();

    // Disable virtual device (mouse emulation on DualSense etc.)
    BP32.enableVirtualDevice(false);

    // Disable BLE service
    BP32.enableBLEService(false);

    LOG_INFO(TAG, "Bluepad32 firmware: %s", BP32.firmwareVersion());

    const uint8_t* addr = BP32.localBdAddress();
    LOG_INFO(TAG, "Bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    LOG_INFO(TAG, "Scanning for controllers...");
}

// BT update rate tracking
static volatile unsigned long s_btUpdateCount = 0;
static unsigned long s_btLastLogMs = 0;

bool ControllerManager::update() {
    // Poll Bluepad32 for new data
    bool dataUpdated = BP32.update();

    if (dataUpdated) {
        s_btUpdateCount++;

        for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
            ControllerPtr ctl = s_rawControllers[i];

            if (ctl && ctl->isConnected() && ctl->hasData() && ctl->isGamepad()) {
                s_states[i].connected = true;
                s_states[i].lx = applyDeadZone(ctl->axisX());
                s_states[i].ly = applyDeadZone(ctl->axisY());
                s_states[i].rx = applyDeadZone(ctl->axisRX());
                s_states[i].ry = applyDeadZone(ctl->axisRY());
                s_states[i].l2 = ctl->brake();
                s_states[i].r2 = ctl->throttle();
                s_states[i].buttons = ctl->buttons();
                s_states[i].miscButtons = ctl->miscButtons();
                s_states[i].dpad = ctl->dpad();
            } else if (s_rawControllers[i] == nullptr) {
                s_states[i].connected = false;
            }
        }
    }

    // Log BT update rate and button state every 2 seconds
    unsigned long now = millis();
    if ((now - s_btLastLogMs) >= 2000) {
        unsigned long count = s_btUpdateCount;
        unsigned long elapsed = now - s_btLastLogMs;
        if (elapsed > 0) {
            unsigned long hz = (count * 1000) / elapsed;
            LOG_INFO(TAG, "BT input rate: %lu Hz (%lu updates in %lu ms)",
                     hz, count, elapsed);
        }
        s_btUpdateCount = 0;
        s_btLastLogMs = now;

        // Dump all button/axis state for the first connected controller
        for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
            if (s_states[i].connected) {
                LOG_INFO(TAG, "Slot%d btns=0x%04X misc=0x%04X dpad=0x%02X L2=%d R2=%d",
                         i, s_states[i].buttons, s_states[i].miscButtons,
                         s_states[i].dpad, s_states[i].l2, s_states[i].r2);
                break;
            }
        }
    }

    return dataUpdated;
}

const ControllerState& ControllerManager::getState(int index) const {
    if (index < 0 || index >= CONTROLLER_MAX_COUNT) {
        static ControllerState emptyState = {};
        return emptyState;
    }
    return s_states[index];
}

int ControllerManager::getConnectedCount() const {
    int count = 0;
    for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
        if (s_states[i].connected) {
            count++;
        }
    }
    return count;
}

int16_t ControllerManager::applyDeadZone(int16_t value) const {
    if (value > -CONTROLLER_DEADZONE && value < CONTROLLER_DEADZONE) {
        return 0;
    }
    return value;
}
