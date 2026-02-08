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
#include "web_log.h"
#include "settings_manager.h"

#include <esp_http_server.h>
#include <ArduinoJson.h>

static const char* TAG = "WebServer";

static httpd_handle_t s_server = NULL;

// External references
extern WiFiManager g_wifiManager;
extern ControllerManager g_controllerManager;
extern DriveManager g_driveManager;
extern MotorManager g_motorManager;
extern SettingsManager g_settingsManager;

// IMU and state machine state (defined in sketch.cpp)
extern volatile bool g_isUpsideDown;
extern float g_pitchAngleForWeb;
extern int g_selfRightStateForWeb;
extern int g_noseDownStateForWeb;

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
        motor["ppSpeed"] = serialized(String(status.ppSpeed, 2));
        motor["ppAccel"] = serialized(String(status.ppAccel, 1));
        motor["limitSpd"] = serialized(String(status.limitSpd, 2));
        motor["limitCur"] = serialized(String(status.limitCur, 2));
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

// Log viewer page
static esp_err_t log_page_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WEB_LOG_HTML, strlen(WEB_LOG_HTML));
    return ESP_OK;
}

// Log streaming JSON endpoint: /logs?since=<seq>
// Returns new log entries since the given sequence number, plus telemetry.
static esp_err_t logs_handler(httpd_req_t* req) {
    // Parse "since" query parameter
    uint32_t sinceSeq = 0;
    char queryBuf[32];
    if (httpd_req_get_url_query_str(req, queryBuf, sizeof(queryBuf)) == ESP_OK) {
        char valBuf[16];
        if (httpd_query_key_value(queryBuf, "since", valBuf, sizeof(valBuf)) == ESP_OK) {
            sinceSeq = (uint32_t)strtoul(valBuf, NULL, 10);
        }
    }

    // Fetch new log entries from ring buffer
    static const int MAX_BATCH = 30;
    static LogEntry entries[MAX_BATCH];  // static to avoid stack overflow
    int count = logRingGetSince(sinceSeq, entries, MAX_BATCH);

    // Build JSON response
    JsonDocument doc;
    doc["head"] = logRingGetHead();

    JsonArray arr = doc["entries"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        arr.add(entries[i].text);
    }

    // Telemetry snapshot
    doc["pitch"] = g_pitchAngleForWeb;
    doc["flipped"] = (bool)g_isUpsideDown;
    doc["sr"] = g_selfRightStateForWeb;
    doc["nd"] = g_noseDownStateForWeb;
    doc["driveL"] = g_driveManager.getLeftDrive();
    doc["driveR"] = g_driveManager.getRightDrive();
    doc["uptime"] = (unsigned long)(millis() / 1000);

    String output;
    serializeJson(doc, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, output.c_str(), output.length());
    return ESP_OK;
}

// Settings data GET - return current modes, presets, and speed limit as JSON
static esp_err_t settingsdata_get_handler(httpd_req_t* req) {
    JsonDocument doc;
    doc["yMode"]  = g_settingsManager.getYMode();
    doc["yLeft"]  = g_settingsManager.getYLeft();
    doc["yRight"] = g_settingsManager.getYRight();
    doc["bMode"]  = g_settingsManager.getBMode();
    doc["bLeft"]  = g_settingsManager.getBLeft();
    doc["bRight"] = g_settingsManager.getBRight();
    doc["aMode"]  = g_settingsManager.getAMode();
    doc["aLeft"]  = g_settingsManager.getALeft();
    doc["aRight"] = g_settingsManager.getARight();
    doc["speedLimit"]   = g_settingsManager.getMotorSpeedLimit();
    doc["acceleration"] = g_settingsManager.getMotorAcceleration();
    doc["currentLimit"] = g_settingsManager.getMotorCurrentLimit();

    String output;
    serializeJson(doc, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, output.c_str(), output.length());
    return ESP_OK;
}

// Settings data POST - update modes, presets, and/or motor tuning
static esp_err_t settingsdata_post_handler(httpd_req_t* req) {
    char buf[512];
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

    // Update Y button config if mode or position provided
    if (doc["yMode"].is<int>() || doc["yLeft"].is<float>()) {
        uint8_t mode = doc["yMode"].is<int>() ? doc["yMode"].as<uint8_t>() : g_settingsManager.getYMode();
        float left = doc["yLeft"].is<float>() ? doc["yLeft"].as<float>() : g_settingsManager.getYLeft();
        float right = doc["yRight"].is<float>() ? doc["yRight"].as<float>() : g_settingsManager.getYRight();
        g_settingsManager.setYConfig(mode, left, right);
    }

    // Update B button config
    if (doc["bMode"].is<int>() || doc["bLeft"].is<float>()) {
        uint8_t mode = doc["bMode"].is<int>() ? doc["bMode"].as<uint8_t>() : g_settingsManager.getBMode();
        float left = doc["bLeft"].is<float>() ? doc["bLeft"].as<float>() : g_settingsManager.getBLeft();
        float right = doc["bRight"].is<float>() ? doc["bRight"].as<float>() : g_settingsManager.getBRight();
        g_settingsManager.setBConfig(mode, left, right);
    }

    // Update A button config
    if (doc["aMode"].is<int>() || doc["aLeft"].is<float>()) {
        uint8_t mode = doc["aMode"].is<int>() ? doc["aMode"].as<uint8_t>() : g_settingsManager.getAMode();
        float left = doc["aLeft"].is<float>() ? doc["aLeft"].as<float>() : g_settingsManager.getALeft();
        float right = doc["aRight"].is<float>() ? doc["aRight"].as<float>() : g_settingsManager.getARight();
        g_settingsManager.setAConfig(mode, left, right);
    }

    // Update motor tuning if provided
    if (doc["speedLimit"].is<float>()) {
        g_settingsManager.setMotorSpeedLimit(doc["speedLimit"].as<float>());
    }
    if (doc["acceleration"].is<float>()) {
        g_settingsManager.setMotorAcceleration(doc["acceleration"].as<float>());
    }
    if (doc["currentLimit"].is<float>()) {
        g_settingsManager.setMotorCurrentLimit(doc["currentLimit"].as<float>());
    }

    // Return success with current state
    JsonDocument resp;
    resp["ok"] = true;
    resp["yMode"]  = g_settingsManager.getYMode();
    resp["yLeft"]  = g_settingsManager.getYLeft();
    resp["yRight"] = g_settingsManager.getYRight();
    resp["bMode"]  = g_settingsManager.getBMode();
    resp["bLeft"]  = g_settingsManager.getBLeft();
    resp["bRight"] = g_settingsManager.getBRight();
    resp["aMode"]  = g_settingsManager.getAMode();
    resp["aLeft"]  = g_settingsManager.getALeft();
    resp["aRight"] = g_settingsManager.getARight();
    resp["speedLimit"]   = g_settingsManager.getMotorSpeedLimit();
    resp["acceleration"] = g_settingsManager.getMotorAcceleration();
    resp["currentLimit"] = g_settingsManager.getMotorCurrentLimit();

    String output;
    serializeJson(resp, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, output.c_str(), output.length());
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
    config.max_uri_handlers = 12;

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

    // Log viewer page
    httpd_uri_t log_page_uri = {};
    log_page_uri.uri     = "/log";
    log_page_uri.method  = HTTP_GET;
    log_page_uri.handler = log_page_handler;
    httpd_register_uri_handler(s_server, &log_page_uri);

    // Log streaming JSON endpoint
    httpd_uri_t logs_uri = {};
    logs_uri.uri     = "/logs";
    logs_uri.method  = HTTP_GET;
    logs_uri.handler = logs_handler;
    httpd_register_uri_handler(s_server, &logs_uri);

    // Settings data GET endpoint
    httpd_uri_t settingsdata_get_uri = {};
    settingsdata_get_uri.uri     = "/settingsdata";
    settingsdata_get_uri.method  = HTTP_GET;
    settingsdata_get_uri.handler = settingsdata_get_handler;
    httpd_register_uri_handler(s_server, &settingsdata_get_uri);

    // Settings data POST endpoint
    httpd_uri_t settingsdata_post_uri = {};
    settingsdata_post_uri.uri     = "/settingsdata";
    settingsdata_post_uri.method  = HTTP_POST;
    settingsdata_post_uri.handler = settingsdata_post_handler;
    httpd_register_uri_handler(s_server, &settingsdata_post_uri);

    _started = true;
    LOG_INFO(TAG, "Web server ready at http://%s:%d/",
             g_wifiManager.getIP().c_str(), WEB_SERVER_PORT);
}

bool WebServerManager::isRunning() const {
    return _started;
}
