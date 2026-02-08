set(srcs
    "main.c"
    "sketch.cpp"
    "debug_log.cpp"
    "wifi_manager.cpp"
    "web_server.cpp"
    "controller_manager.cpp"
    "drive_manager.cpp"
    "display_manager.cpp"
    "motor_manager.cpp"
    "settings_manager.cpp")

set(requires "bluepad32" "bluepad32_arduino" "arduino" "btstack" "esp_http_server" "driver" "esp_coex" "nvs_flash")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
