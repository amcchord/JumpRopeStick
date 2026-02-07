# M5StickC Plus 2 - Hardware Reference

## Overview

- **SKU**: K016-P2
- **SoC**: ESP32-PICO-V3-02 (Dual-Core Xtensa LX6, up to 240MHz)
- **Flash**: 8MB
- **PSRAM**: 2MB
- **Wi-Fi**: 2.4 GHz 802.11 b/g/n
- **Bluetooth**: Classic (BR/EDR) + BLE 4.2
- **Battery**: 200mAh @ 3.7V lithium
- **Display**: 1.14" TFT LCD, 135x240, ST7789V2 driver
- **IMU**: MPU6886 (3-axis accelerometer + 3-axis gyroscope)
- **RTC**: BM8563
- **Microphone**: SPM1423
- **IR Emitter**: G19 (shared with Red LED)
- **Buzzer**: Passive buzzer on G2
- **USB**: Type-C (CH9102 USB-UART)

## Pin Map

### GPIO Usage in This Project

| GPIO | Function | Notes |
|------|----------|-------|
| G4   | HOLD pin | Must set HIGH in setup() to keep power on |
| G25  | Servo PPM Left Wheel | External header pin |
| G26  | Servo PPM Right Wheel | External header pin |
| G32  | CAN TX (Mini CAN Unit) | HY2.0-4P Grove port (Yellow wire) |
| G33  | CAN RX (Mini CAN Unit) | HY2.0-4P Grove port (White wire) |
| G21  | I2C SDA | MPU6886 IMU + BM8563 RTC |
| G22  | I2C SCL | MPU6886 IMU + BM8563 RTC |
| G37  | Button A | Active LOW |
| G39  | Button B | Active LOW |
| G35  | Button C (Power) | Press >2s to power on, >6s to power off |

### Display (ST7789V2) Pins

| GPIO | Function |
|------|----------|
| G15  | TFT_MOSI |
| G13  | TFT_CLK  |
| G14  | TFT_DC   |
| G12  | TFT_RST  |
| G5   | TFT_CS   |
| G27  | TFT_BL (Backlight) |

### Other Pins

| GPIO | Function |
|------|----------|
| G19  | IR Emitter + Red LED (shared) |
| G0   | Microphone CLK |
| G34  | Microphone DATA |
| G2   | Passive Buzzer |
| G38  | Battery voltage detect (ADC) |
| G36  | Available on external header |

### HY2.0-4P Grove Port

| Pin    | Color  | GPIO |
|--------|--------|------|
| GND    | Black  | -    |
| 5V     | Red    | -    |
| Signal | Yellow | G32  |
| Signal | White  | G33  |

## Power Management

The M5StickC Plus 2 does NOT use the AXP192 PMIC (unlike the original StickC).
Instead, power is controlled via the HOLD pin:

- **Power ON**: Press Button C for >2 seconds, or RTC IRQ wake
- **Power OFF** (no USB): Press Button C >6 seconds, or set HOLD (G4) = LOW
- **Power OFF** (USB connected): Button C >6 seconds enters sleep mode (screen off)
- **CRITICAL**: After boot, firmware MUST set G4 HIGH to keep the device powered.
  If G4 is not set HIGH, the device will shut down immediately after the button is released.

```cpp
// Must be called early in setup()
pinMode(GPIO_NUM_4, OUTPUT);
digitalWrite(GPIO_NUM_4, HIGH);
```

## Display Specifications

- **Driver**: ST7789V2
- **Resolution**: 135 x 240 pixels
- **Color depth**: 16-bit (RGB565)
- **Interface**: SPI
- **Orientation**: Portrait by default, can be rotated in software

## IMU - MPU6886

- **Accelerometer**: +/-2g, +/-4g, +/-8g, +/-16g
- **Gyroscope**: +/-250, +/-500, +/-1000, +/-2000 dps
- **Interface**: I2C (address 0x68)
- **Data rate**: Up to 1kHz
- **Shared I2C bus** with RTC (BM8563 at address 0x51)

## Arduino / PlatformIO Library

The official library is **M5Unified** which provides a unified API across all M5Stack devices:
- Repository: https://github.com/m5stack/M5Unified
- Handles display, IMU, buttons, power, RTC, buzzer
- Auto-detects device type

Legacy library (M5StickCPlus2) also exists but M5Unified is recommended.

## Datasheets

- ESP32-PICO-V3-02: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/669/esp32-pico_series_datasheet_en.pdf
- ST7789V2: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/ST7789V.pdf
- MPU6886: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/MPU-6886-000193+v1.1_GHIC_en.pdf
- BM8563 RTC: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/BM8563_V1.1_cn.pdf
- Schematics: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/512/Sch_M5StickC_Plus2_v0.5.pdf
