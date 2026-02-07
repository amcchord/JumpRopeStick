// =============================================================================
// JumpRopeStick - Bluepad32 / BTstack Initialization
// =============================================================================
// This file initializes the Bluetooth stack and Bluepad32 platform.
// It runs on CPU0. Arduino setup() and loop() run on CPU1.
//
// DO NOT add application logic here. Use sketch.cpp for that.
// =============================================================================

#include "sdkconfig.h"

#include <stddef.h>

// BTstack related
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>

// Bluepad32 related
#include <arduino_platform.h>
#include <uni.h>

//
// Entry point - Bluepad32 console is DISABLED (we use Arduino Serial instead).
// See sdkconfig.defaults: CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n
//
#if CONFIG_AUTOSTART_ARDUINO
void initBluepad32() {
#else
int app_main(void) {
#endif

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // Set Arduino as the Bluepad32 platform (provides setup/loop callbacks)
    uni_platform_set_custom(get_arduino_platform());

    // Initialize Bluepad32
    uni_init(0 /* argc */, NULL /* argv */);

    // Run the BTstack event loop (does not return)
    btstack_run_loop_execute();

#if !CONFIG_AUTOSTART_ARDUINO
    return 0;
#endif
}
