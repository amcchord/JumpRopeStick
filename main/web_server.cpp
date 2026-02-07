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
#include "drive_manager.h"
#include "motor_manager.h"
#include "web_ui.h"
#include "web_config.h"

#include <esp_http_server.h>
#include <ArduinoJson.h>

static const char* TAG = "WebServer";

static httpd_handle_t s_server = NULL;

// External references
extern WiFiManager g_wifiManager;
extern ControllerManager g_controllerManager;
extern DriveManager g_driveManager;
extern MotorManager g_motorManager;

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
            ctrl["l2"] = state.l2;
            ctrl["r2"] = state.r2;
            ctrl["buttons"] = state.buttons;
            ctrl["dpad"] = state.dpad;
        }
    }

    // Drive outputs (servo PPM)
    JsonObject drive = doc["drive"].to<JsonObject>();
    drive["left"] = g_driveManager.getLeftPulse();
    drive["right"] = g_driveManager.getRightPulse();
    drive["leftDrive"] = serialized(String(g_driveManager.getLeftDrive(), 2));
    drive["rightDrive"] = serialized(String(g_driveManager.getRightDrive(), 2));

    // CAN Motors
    JsonArray motors = doc["motors"].to<JsonArray>();
    int motorCount = g_motorManager.getMotorCount();
    for (int i = 0; i < motorCount; i++) {
        const RobstrideMotorStatus& status = g_motorManager.getMotorStatus(i);
        JsonObject motor = motors.add<JsonObject>();
        uint8_t motorCanId = g_motorManager.getMotorId(i);
        motor["id"] = motorCanId;
        motor["role"] = g_motorManager.getRoleLabel(motorCanId);
        motor["position"] = serialized(String(status.position, 3));
        motor["velocity"] = serialized(String(status.velocity, 2));
        motor["torque"] = serialized(String(status.torque, 2));
        motor["temperature"] = serialized(String(status.temperature, 1));
        motor["voltage"] = serialized(String(status.voltage, 1));
        motor["mode"] = status.mode;
        motor["runMode"] = status.runMode;
        motor["enabled"] = status.enabled;
        motor["errorCode"] = status.errorCode;
        motor["hasFault"] = status.hasFault;
        motor["stale"] = status.stale;
    }
    doc["canRunning"] = g_motorManager.isRunning();

    // Motor role config
    JsonObject motorConfig = doc["motorConfig"].to<JsonObject>();
    motorConfig["leftId"] = g_motorManager.getLeftMotorId();
    motorConfig["rightId"] = g_motorManager.getRightMotorId();

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

// Config GET - return current motor config as JSON
static esp_err_t config_get_handler(httpd_req_t* req) {
    JsonDocument doc;
    doc["leftId"] = g_motorManager.getLeftMotorId();
    doc["rightId"] = g_motorManager.getRightMotorId();

    // Include list of discovered motor IDs for the dropdown
    JsonArray discovered = doc["discovered"].to<JsonArray>();
    int count = g_motorManager.getMotorCount();
    for (int i = 0; i < count; i++) {
        discovered.add(g_motorManager.getMotorId(i));
    }

    String output;
    serializeJson(doc, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, output.c_str(), output.length());
    return ESP_OK;
}

// Config POST - update motor role assignments
static esp_err_t config_post_handler(httpd_req_t* req) {
    // Read body (limit to 256 bytes)
    char buf[256];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, buf);
    if (err) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Update motor roles if provided
    if (doc.containsKey("leftId")) {
        g_motorManager.setLeftMotorId(doc["leftId"].as<uint8_t>());
    }
    if (doc.containsKey("rightId")) {
        g_motorManager.setRightMotorId(doc["rightId"].as<uint8_t>());
    }

    // Return updated config
    JsonDocument resp;
    resp["leftId"] = g_motorManager.getLeftMotorId();
    resp["rightId"] = g_motorManager.getRightMotorId();
    resp["ok"] = true;

    String output;
    serializeJson(resp, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, output.c_str(), output.length());
    return ESP_OK;
}

// Settings page - serves the configuration UI
static esp_err_t settings_handler(httpd_req_t* req) {
    extern const char WEB_CONFIG_HTML[];
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WEB_CONFIG_HTML, strlen(WEB_CONFIG_HTML));
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
    config.max_uri_handlers = 10;

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

    // Config GET endpoint
    httpd_uri_t config_get_uri = {};
    config_get_uri.uri     = "/config";
    config_get_uri.method  = HTTP_GET;
    config_get_uri.handler = config_get_handler;
    httpd_register_uri_handler(s_server, &config_get_uri);

    // Config POST endpoint
    httpd_uri_t config_post_uri = {};
    config_post_uri.uri     = "/config";
    config_post_uri.method  = HTTP_POST;
    config_post_uri.handler = config_post_handler;
    httpd_register_uri_handler(s_server, &config_post_uri);

    // Settings page
    httpd_uri_t settings_uri = {};
    settings_uri.uri     = "/settings";
    settings_uri.method  = HTTP_GET;
    settings_uri.handler = settings_handler;
    httpd_register_uri_handler(s_server, &settings_uri);

    _started = true;
    LOG_INFO(TAG, "Web server ready at http://%s:%d/",
             g_wifiManager.getIP().c_str(), WEB_SERVER_PORT);
}

bool WebServerManager::isRunning() const {
    return _started;
}
