# HVAC NanoSensorHub

Arduino Nano (ATmega328P) sensor hub coprocessor for the HVAC & IAQ Monitor project.

---

## Overview

The NanoSensorHub handles real-time low-level sensor sampling, pulse counting, analog conversions, and I2C/UART sensor interfaces:
- **Sensirion SPS30**: Particulate matter (PM1.0, PM2.5, PM4.0, PM10.0)
- **Sensirion SCD30**: NDIR CO2, Temperature & Humidity
- **Sensirion SGP41**: VOC & NOx gas indices
- **Geiger Counter**: Background radiation pulse detection
- **Pressure Sensors**: Filter and differential pressure ADCs
- **Current Clamps**: Blower fan, compressor, and geothermal pump current sensing

Telemetry packets and command responses are exchanged with the ESP32 over UART at 19,200 baud using a checksummed framed protocol (`<data,checksum>`).

---

## Bootloader & Wireless Reflashing

This device runs a custom **`urboot`** bootloader that allows the ESP32 to trigger an out-of-band software reboot into programming mode and stream firmware updates over UART wirelessly via Web OTA.

For comprehensive instructions on the bootloader implementation, building from source, burning with an ISP programmer, and fuse settings, see:
👉 **[BOOTLOADER.md](BOOTLOADER.md)**

---

## Building and Uploading

### Compiling
```bash
pio run
```

### Uploading via Nano USB Port
Because the board uses the custom `urboot` bootloader, the upload protocol in `platformio.ini` is set to `urclock`:
```bash
pio run -t upload
```

### Uploading Wirelessly via ESP32 Web OTA
Upload `HVAC_NanoSensorHub/.pio/build/minicore_328p/firmware.hex` directly through the ESP32 Web OTA portal at `http://<esp32-ip>/ota`.
