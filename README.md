# HVAC Filter Monitor & Air Quality Sensor

This project is a comprehensive monitoring system for a home HVAC (Heating, Ventilation, and Air Conditioning) unit. It combines an ESP32-based touch display with an Arduino Nano sensor hub to provide real-time data on HVAC performance and indoor air quality. All data is integrated into Home Assistant via MQTT for easy monitoring and automation.

## Overview

The system is composed of two main components:

1.  **ESP32 Display Controller**: An ESP32-2432S022C board runs a rich user interface built with LVGL. It handles WiFi connectivity, communicates with the sensor hub through uart, and pushes all data to a Home Assistant instance via MQTT. It also supports Over-the-Air (OTA) updates via both ArduinoOTA and a web interface.

2.  **Arduino Nano Sensor Hub**: An Arduino Nano (ATmega168P) acts as a dedicated I2C and analog sensor hub. It gathers data from a suite of sensors and sends it to the ESP32 over a robust serial protocol. It runs a custom non-blocking firmware and uses the **Urboot bootloader** for maximum code space availability and seemless LTO optimized firmware binary.

## Features

- **HVAC Monitoring**:
  - Measures differential pressure across the air filter to determine its condition.
  - Monitors the fan's electrical current using a CT clamp to detect its operational state (Off, Normal, Alert).
  - Provides a "High Pressure" alert, indicating a clogged filter that needs replacement.

- **Air Quality Sensing**:
  - **Particulate Matter**: Sensirion SPS30 for PM1.0, PM2.5, PM4.0, and PM10 readings.
  - **CO₂ Levels**: Sensirion SCD30 for Carbon Dioxide concentration.
  - **VOC Index**: Sensirion SGP41 for a Volatile Organic Compounds index value.
  - **Temperature & Humidity**: Provided by the Sensirion SCD30.

- **Environmental Sensing**:
  - **Radiation**: A Geiger counter measures background radiation in Counts Per Minute (CPM) and calculates the dose in µSv/h.

- **User Interface**:
  - A 2.2" color touch screen powered by LVGL.
  - Multiple swipeable tiles to display different categories of data (Air Quality, HVAC Status, System Info).
  - Status icons for WiFi, Home Assistant connection, sensor hub connection, and alerts.

- **Home Assistant Integration**:
  - Automatic discovery of all sensors and controls via MQTT.
  - Entities include sensors for all measured values, binary sensors for alerts, and buttons for system control.
  - Diagnostic sensors for Nano SensorStack subsystem's uptime, free RAM, and last reset cause.

- **System & Diagnostics**:
  - Uart/Serial communication between ESP32 and Nano with checksum validation.
  - Remote logging to a Syslog server with an offline message queue.
  - OTA updates for the ESP32.

## Hardware

### Main Components
- **Display Controller**: [Sunton ESP32-2432S022C](https://www.sunton.com.cn/product-page/esp32-2-2-inch-240-320-tft-with-touch) (ESP32-WROOM-32, 2.2" ST7789 TFT, CST820 Touch)
- **Sensor Hub**: Arduino Nano (ATmega168P @ 16MHz)

### Sensors
- **Differential Pressure**: Custom 4-20mA current loop sensor.
- **Particulate Matter**: [Sensirion SPS30](https://sensirion.com/products/catalog/SPS30/)
- **CO₂, Temp, Humidity**: [Sensirion SCD30](https://sensirion.com/products/catalog/SCD30/)
- **VOC**: [Sensirion SGP41](https://sensirion.com/products/catalog/SGP41/)
- **Fan Current**: SCT-013-010 Non-invasive AC Current Sensor (CT Clamp)
- **Radiation**: Generic Geiger-Müller tube with a pulse output.

## Software & Libraries

This project is built using [PlatformIO](https://platformio.org/).

### ESP32 (Display Controller)
- **Framework**: Arduino for ESP32
- **UI**: [LVGL (Light and Versatile Graphics Library)](https://lvgl.io/)
- **Display Driver**: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- **Home Assistant**: [ArduinoHA](https://github.com/dawidchyrzynski/arduino-home-assistant)
- **Logging**: `Syslog`, `ESP32Ping`
- **Updates**: `ArduinoOTA`, `WebServer`

### Arduino Nano (Sensor Hub)
- **Core**: MiniCore
- **Bootloader**: **Urboot**. Refer to [urboot Github page](https://github.com/stefanrueger/urboot) for more information.
- **Sensor Libraries**: Official Sensirion libraries for SPS30, SCD30, and SGP41.

## Communication Protocol (ESP32 <-> Nano)

Communication occurs over a simple, packet-based serial protocol at 19200 baud.

- **Packet Format**: `<DATA,CHECKSUM>`
- **Checksum**: A custom 8-bit CRC (polynomial `0x07`) ensures data integrity.
- **Commands**: The ESP32 sends single-character commands to the Nano to request data or actions (e.g., `H` for health, `V` for version, `R` for reboot).
- **Responses**: The Nano responds with data packets prefixed with a character identifying the payload type (e.g., `S` for sensor broadcast, `h` for health response).

## Setup & Installation

### 1. Prerequisites
- PlatformIO Core installed in your IDE (e.g., VSCode).
- An ISP programmer for the initial bootloader flash on the Arduino Nano (e.g., USBasp, another Arduino as ISP).

### 2. Configuration

Copy the `include/secrets.h.example` file to `include/secrets.h` and fill in your credentials:

```cpp
// include/secrets.h

#define WIFI_SSID "YourWiFi_SSID"
#define WIFI_PASSWORD "YourWiFi_Password"

#define MQTT_HOST "192.168.1.100" // IP or hostname of your MQTT broker
#define MQTT_USER "mqtt_username"
#define MQTT_PASSWORD "mqtt_password"
```

### 3. Building and Uploading

This project contains two separate PlatformIO environments: one for the ESP32 and one for the Nano.

#### Arduino Nano (Sensor Hub)

The Nano requires a one-time setup to install the Urboot bootloader.

1.  **Burn Bootloader (First Time Only)**:
    - Connect your ISP programmer to the Nano's ICSP header.
    - Program urboot compiled bootloader to your device
      - [urboot_m168p_2s_autobaud_uart0_rxd0_txd1_led+b5_hw.hex](https://raw.githubusercontent.com/stefanrueger/urboot.hex/main/mcus/atmega168p/watchdog_2_s/autobaud/uart0_rxd0_txd1/led%2Bb5/urboot_m168p_2s_autobaud_uart0_rxd0_txd1_led%2Bb5_hw.hex) was used for this project.
    - Program fuses
      - For ATMega168PA with 256bytes urboot bootloader with hw reset vector
        ```
        lfuse: 0xf7
        hfuse: 0xd4
        efuse: 0xfe
        ```

2.  **Upload Firmware**:
    - After the bootloader is installed, you can upload the firmware over the regular USB serial connection.
    - Use the `Upload_UART` environment.
      ```bash
      pio run -e Upload_UART -t upload
      ```

#### ESP32 (Display Controller)

Uploading to the ESP32 is straightforward.

1.  **Upload Firmware**:
    - Connect the ESP32 board via USB.
    - Run the standard PlatformIO upload command.
      ```bash
      pio run -t upload
      ```

2.  **OTA Updates**:
    - Once the initial firmware is flashed on ESP32, subsequent updates can be done over the network using either the PlatformIO OTA command or the built-in web updater.
    - For Arduino Nano, manual firmware flashing is still required

## Home Assistant Integration

Once the device is connected to your network and MQTT broker, it will automatically appear in Home Assistant under the "HVAC Sensor Display" device.

**Entities Created:**
- **Sensors**: Differential Pressure, Temperature, Humidity, Fan Current, CO₂, VOC Index, PM1.0, PM2.5, PM4.0, PM10.0, Geiger CPM, Geiger Dose Rate.
- **Diagnostic Sensors**: WiFi RSSI, Sensor Stack Uptime, Sensor Stack Free RAM, Sensor Stack Firmware Version, Sensor Stack Reset Cause.
- **Binary Sensors**: Fan Status, High Pressure Alert, Sensor Stack Connection.
- **Controls**:
  - A `HALight` entity to control the display backlight brightness.
  - A `HAButton` to reboot the ESP32 display.
  - A `HAButton` to remotely reboot the Nano sensor stack.
  - A `HAButton` to request detailed diagnostic info from the SPS30 sensor.
  - A `HAButton` to trigger the SGP41 self-test.

---

*This project is a work in progress. Contributions and suggestions are welcome!*