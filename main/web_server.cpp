// =============================================================================
// Web Server Module - Implementation
// =============================================================================
// Simple HTTP server with polling-based status endpoint.
// The web UI fetches /status at 10Hz via fetch().
// =============================================================================

#include "web_server.h"
#include "config.h"
#include "debug_log.h"
#include "wifi_manager.h"
#include "controller_manager.h"
#include "web_ui.h"

#include <esp_http_server.h>
#include <ArduinoJson.h>

static const char* TAG = "WebServer";

static httpd_handle_t s_server = NULL;

// External references
extern WiFiManager g_wifiManager;
extern ControllerManager g_controllerManager;

// ---------------------------------------------------------------------------
// Build JSON status string
// ---------------------------------------------------------------------------
static String buildStatusJson() {
    JsonDocument doc;

    // WiFi status
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = g_wifiManager.getSSID();
    wifi["ip"] = g_wifiManager.getIP();
    wifi["rssi"] = g_wifiManager.getRSSI();

    // Controller states
    JsonArray controllers = doc["controllers"].to<JsonArray>();
    for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
        const ControllerState& state = g_controllerManager.getState(i);
        JsonObject ctrl = controllers.add<JsonObject>();
        ctrl["id"] = i;
        ctrl["connected"] = state.connected;
        if (state.connected) {
            ctrl["model"] = state.modelName;
            ctrl["lx"] = state.lx;
            ctrl["ly"] = state.ly;
            ctrl["rx"] = state.rx;
            ctrl["ry"] = state.ry;
            ctrl["lt"] = state.lt;
            ctrl["rt"] = state.rt;
            ctrl["buttons"] = state.buttons;
            ctrl["dpad"] = state.dpad;
        }
    }

    // Servo outputs (placeholder for future use)
    JsonObject servos = doc["servos"].to<JsonObject>();
    servos["left"] = 1500;
    servos["right"] = 1500;

    // System info
    JsonObject sys = doc["system"].to<JsonObject>();
    sys["uptime_s"] = (unsigned long)(millis() / 1000);
    sys["free_heap"] = ESP.getFreeHeap();
    sys["free_psram"] = ESP.getFreePsram();

    String output;
    serializeJson(doc, output);
    return output;
}

// ---------------------------------------------------------------------------
// HTTP Handlers
// ---------------------------------------------------------------------------

static esp_err_t root_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WEB_UI_HTML, strlen(WEB_UI_HTML));
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t* req) {
    String json = buildStatusJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

static esp_err_t health_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "ok", 2);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void WebServerManager::begin() {
    if (_started) {
        return;
    }

    LOG_INFO(TAG, "Starting web server on port %d...", WEB_SERVER_PORT);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 8;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG, "Failed to start httpd: %s", esp_err_to_name(ret));
        return;
    }

    LOG_INFO(TAG, "httpd started, registering handlers...");

    // Root - dashboard HTML
    httpd_uri_t root_uri = {};
    root_uri.uri     = "/";
    root_uri.method  = HTTP_GET;
    root_uri.handler = root_handler;
    httpd_register_uri_handler(s_server, &root_uri);

    // Status JSON endpoint (polled by the UI)
    httpd_uri_t status_uri = {};
    status_uri.uri     = "/status";
    status_uri.method  = HTTP_GET;
    status_uri.handler = status_handler;
    httpd_register_uri_handler(s_server, &status_uri);

    // Health check
    httpd_uri_t health_uri = {};
    health_uri.uri     = "/health";
    health_uri.method  = HTTP_GET;
    health_uri.handler = health_handler;
    httpd_register_uri_handler(s_server, &health_uri);

    _started = true;
    LOG_INFO(TAG, "Web server ready at http://%s:%d/",
             g_wifiManager.getIP().c_str(), WEB_SERVER_PORT);
}

bool WebServerManager::isRunning() const {
    return _started;
}
