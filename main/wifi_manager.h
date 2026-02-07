#pragma once

// =============================================================================
// WiFi Manager Module
// =============================================================================
// Handles WiFi station connection with automatic reconnect and exponential
// backoff. Non-blocking - never delays the main loop.
//
// Usage:
//   WiFiManager wifi;
//   wifi.begin();           // Call once in setup()
//   wifi.loop();            // Call every iteration of loop()
//   if (wifi.isConnected()) { ... }
// =============================================================================

#include <Arduino.h>

class WiFiManager {
public:
    // Initialize WiFi in station mode and start connecting.
    // Reads SSID/password from config.h.
    void begin();

    // Must be called every loop iteration.
    // Handles reconnection with exponential backoff.
    void loop();

    // Returns true if WiFi is connected and has an IP address.
    bool isConnected() const;

    // Returns the current IP address as a string (or "0.0.0.0" if not connected).
    String getIP() const;

    // Returns the WiFi RSSI (signal strength) in dBm.
    // Returns 0 if not connected.
    int getRSSI() const;

    // Returns the SSID we are configured to connect to.
    const char* getSSID() const;

private:
    enum class State {
        IDLE,
        CONNECTING,
        CONNECTED,
        DISCONNECTED
    };

    State _state = State::IDLE;
    unsigned long _lastAttemptMs = 0;
    unsigned long _backoffMs = 0;
    unsigned long _connectStartMs = 0;
    bool _wasConnected = false;
};
