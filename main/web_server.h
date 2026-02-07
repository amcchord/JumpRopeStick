#pragma once

// =============================================================================
// Web Server Module
// =============================================================================
// Uses ESP-IDF native httpd. Serves the embedded web UI at "/" and provides
// a JSON status endpoint at /status that the UI polls at 4Hz.
//
// No WebSocket complexity - just simple HTTP GET requests.
//
// Usage:
//   WebServerManager webServer;
//   webServer.begin();   // Call once in setup() after WiFi connects
// =============================================================================

#include <Arduino.h>

class WebServerManager {
public:
    // Start the HTTP server.
    // Should be called after WiFi is connected.
    void begin();

    // Returns true if the server was started successfully.
    bool isRunning() const;

private:
    bool _started = false;
};
