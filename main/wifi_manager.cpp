// =============================================================================
// WiFi Manager Module - Implementation
// =============================================================================

#include "wifi_manager.h"
#include "config.h"
#include "debug_log.h"
#include <WiFi.h>

static const char* TAG = "WiFi";

void WiFiManager::begin() {
    LOG_INFO(TAG, "Initializing WiFi...");
    LOG_INFO(TAG, "SSID: %s", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection ourselves

    // Start first connection attempt
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    _state = State::CONNECTING;
    _connectStartMs = millis();
    _backoffMs = WIFI_RECONNECT_INTERVAL_MIN_MS;
    _lastAttemptMs = millis();

    LOG_INFO(TAG, "Connecting...");
}

void WiFiManager::loop() {
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    switch (_state) {
        case State::IDLE:
            // Should not happen after begin() is called
            break;

        case State::CONNECTING: {
            if (currentlyConnected) {
                _state = State::CONNECTED;
                _wasConnected = true;
                _backoffMs = WIFI_RECONNECT_INTERVAL_MIN_MS;
                LOG_INFO(TAG, "Connected! IP: %s", WiFi.localIP().toString().c_str());
                LOG_INFO(TAG, "RSSI: %d dBm", WiFi.RSSI());
            } else {
                // Check for connection timeout
                unsigned long elapsed = millis() - _connectStartMs;
                if (elapsed > WIFI_CONNECT_TIMEOUT_MS) {
                    LOG_WARN(TAG, "Connection attempt timed out after %lu ms", elapsed);
                    WiFi.disconnect();
                    _state = State::DISCONNECTED;
                    _lastAttemptMs = millis();
                }
            }
            break;
        }

        case State::CONNECTED: {
            if (!currentlyConnected) {
                LOG_WARN(TAG, "Connection lost!");
                _state = State::DISCONNECTED;
                _lastAttemptMs = millis();
                _backoffMs = WIFI_RECONNECT_INTERVAL_MIN_MS;
            }
            break;
        }

        case State::DISCONNECTED: {
            // Exponential backoff reconnection
            unsigned long elapsed = millis() - _lastAttemptMs;
            if (elapsed >= _backoffMs) {
                LOG_INFO(TAG, "Reconnecting... (backoff: %lu ms)", _backoffMs);
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                _state = State::CONNECTING;
                _connectStartMs = millis();

                // Double the backoff for next time, capped at max
                _backoffMs = _backoffMs * 2;
                if (_backoffMs > WIFI_RECONNECT_INTERVAL_MAX_MS) {
                    _backoffMs = WIFI_RECONNECT_INTERVAL_MAX_MS;
                }
            }
            break;
        }
    }
}

bool WiFiManager::isConnected() const {
    return (_state == State::CONNECTED) && (WiFi.status() == WL_CONNECTED);
}

String WiFiManager::getIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int WiFiManager::getRSSI() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

const char* WiFiManager::getSSID() const {
    return WIFI_SSID;
}
