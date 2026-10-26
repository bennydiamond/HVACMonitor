# Custom urboot Bootloader for ATmega328P Nano Sensor Hub

This document details the custom `urboot` bootloader implementation, how it works, how to rebuild it from source, and how to flash it to the ATmega328P.

---

## 1. Overview & Architecture

The Arduino Nano serves as a coprocessor to the main ESP32. Because only UART RX and TX wires connect the ESP32 and Nano (there is no physical GPIO reset wire), the ESP32 must be able to reboot the Nano and hold it in the bootloader purely via software over serial.

### Bootloader Modifications (`urboot/src/urboot.c`)

1. **Dual 16-Bit Complementary Magic Keys (`MAGIC_BOOT`)**:
   - Location: Stored in the top 4 bytes of SRAM (`RAMEND - 3` to `RAMEND`).
   - **Key 1**: `0xB007` at `0x08FC` (SRAM byte addresses `0x08FC - 0x08FD`).
   - **Key 2**: `0x4FF8` (`~0xB007`, bitwise complement) at `0x08FE` (SRAM byte addresses `0x08FE - 0x08FF`).
   - Both keys must match precisely for the bootloader to stay in programming mode.

2. **Power-On & Brownout Protection**:
   - Checks `MCUSR` flags:
     ```c
     if (!(mcusr & (_BV(PORF) | _BV(BORF))) && (mcusr & _BV(WDRF))) {
         // Only enter if triggered by watchdog, never on cold power-on or brownout
     }
     ```
   - Prevents uninitialized RAM contents after power loss or brownouts from accidentally triggering the bootloader.

3. **Immediate RAM Key Invalidation**:
   - The bootloader wipes both keys (`*BOOT_KEY_ADDR = 0; *BOOT_INV_ADDR = 0;`) upon boot.
   - If the ESP32 fails to start flashing within 1 second, the 1-second watchdog fires, resets the Nano, and because the keys are zeroed, the Nano immediately launches the main application.

4. **Reset Reason Preservation (`r2`)**:
   - The bootloader preserves `MCUSR` in register `r2` across the bootloader trampoline so the application sketch's `.init0` handler (`urboot_reset_flags`) can continue reading and reporting the exact reset cause (`Power-On`, `Brown-Out`, `External`, `Watchdog`).

5. **Fail-Safe Glitch & Power-Loss Protection (`VBL=1`)**:
   - Before jumping to the application via `rjmp_application` (`rjmp 0x8064` wrapping to vector 25 at `0x0064`), `urboot` inspects the flash word at vector 25 (`0x0064`).
   - If vector 25 is unwritten (`0xFFFF`), the bootloader will NOT jump to the application, but instead remains in bootloader mode.
   - Combined with `NanoFlasher`'s staged write (which leaves vector 25 as `0xFFFFFFFF` until all other pages are fully written and verified), an interrupted or failed flash update never bricks the board: it automatically falls back into `urboot` on reset so the ESP32 can retry.

6. **Hardware Features Enabled**:
   - **`AUTOBAUD=1`**: Hardware bit timing on the falling edge of `0x30` (`STK_GET_SYNC`), locking onto any host baud rate (19,200 baud, 115,200 baud, etc.).
   - **`EEPROM=1`**: Enables read (`0x01`) and write (`0x00`) access to Nano EEPROM via `urprotocol`.
   - **Compiled Size**: 372 bytes (comfortably fits in the 384/512-byte section at `0x7E80 - 0x7FFF`).

---

## 2. Generating the Bootloader (`.hex`)

The bootloader build environment uses a Docker container wrapping toolchain `avr-gcc` 7.3.0 and the Perl preprocessing scripts.

### Build Steps

1. **Build the Docker Image** (from the `urboot/` directory):
   ```bash
   cd HVAC_NanoSensorHub/urboot
   docker build -t urboot-builder .
   ```

2. **Compile the Hex File**:
   ```bash
   docker run --rm -v $(pwd)/src:/src urboot-builder \
       MCU=atmega328p \
       AUTOBAUD=1 \
       LED=AtmelPB5 \
       VBL=1 \
       NAME=urboot_atmega328p_magic_autobaud
   ```

3. **Output Files** (generated in `urboot/src/`):
   - `urboot_atmega328p_magic_autobaud.hex` (Intel HEX to program)
   - `urboot_atmega328p_magic_autobaud.elf` (ELF binary)
   - `urboot_atmega328p_magic_autobaud.lst` (Disassembly listing)

---

## 3. Programming the Bootloader via Arduino Uno as ISP

Because a microcontroller cannot overwrite its own bootloader over serial, the bootloader must be programmed via an ISP programmer connected to the Nano's ICSP header.

### Hardware Wiring

| Arduino Uno (Programmer) | Arduino Nano (Target) | Wire Function |
| :--- | :--- | :--- |
| **5V** | **5V** | Power |
| **GND** | **GND** | Ground |
| **Pin 10** | **RESET** (Pin 28 or RST) | Target Reset |
| **Pin 11** | **Pin 11** (or ICSP Pin 4) | MOSI |
| **Pin 12** | **Pin 12** (or ICSP Pin 1) | MISO |
| **Pin 13** | **Pin 13** (or ICSP Pin 3) | SCK |

> **IMPORTANT**: Upload the `ArduinoISP` example sketch to the Uno first (File → Examples → 11.ArduinoISP). Then connect a **10 µF capacitor** between the Uno's **RESET** (+) and **GND** (-) to prevent the Uno from auto-resetting when `avrdude` connects.

### Burning Fuses and Bootloader

Run from the `HVACMonitor` root directory:

```bash
avrdude -c stk500v1 -p m328p -P /dev/ttyACM0 -b 19200 \
  -U lock:w:0x3F:m \
  -U efuse:w:0xFD:m \
  -U hfuse:w:0xD7:m \
  -U lfuse:w:0xFF:m \
  -U flash:w:HVAC_NanoSensorHub/urboot/src/urboot_atmega328p_magic_autobaud.hex:i \
  -U lock:w:0xFF:m
```

*(Replace `/dev/ttyACM0` with your Uno's serial port if different, e.g. `/dev/ttyUSB0`)*.

#### Fuse Meanings:
- **`hfuse = 0xD7`**: `BOOTSZ = 11` (512-byte boot size `0x7E00 - 0x7FFF`), `BOOTRST = 1` (unprogrammed: the CPU starts at `0x0000`).
  - **Invariant: every reset (power-on, brown-out, external/DTR, watchdog) goes through the bootloader.** The reset vector at `0x0000` always holds `jmp 0x7E80` (`0C 94 40 3F`), and urboot then decides whether to stay or start the app via vector 25 (`0x0064`).
  - The bootloader does not enforce this itself (this build has no room for `PROTECTRESET`), so every uploader must write that reset vector: `avrdude -c urclock` and the ESP32 `NanoFlasher` both do.
- **`lfuse = 0xFF`**: 16 MHz external crystal oscillator with 65 ms startup delay.
- **`efuse = 0xFD`**: 2.7V Brown-Out Detection (BOD).
- **`lock = 0xFF`**: Unlocked (urboot internally protects its own memory with `PROTECTME=1`).

---

## 4. PlatformIO Configuration (`platformio.ini`)

To upload sketches directly to the Nano via its USB port, `platformio.ini` **must** specify `upload_protocol = urclock`:

```ini
[env:minicore_328p]
platform = atmelavr
board = ATmega328P
framework = arduino
upload_protocol = urclock       ; Required: avrdude -c urclock for urprotocol
monitor_speed = 19200
board_build.f_cpu = 16000000L
board_hardware.oscillator = external
board_bootloader.type = urboot
board_hardware.uart = uart0
board_upload.speed = 115200
```

---

## 5. Wireless Reflashing via ESP32 Web OTA

Once this bootloader is installed, you no longer need physical access or an ISP programmer to update the Nano.

1. Build the Nano sketch:
   ```bash
   cd HVAC_NanoSensorHub
   pio run
   ```
2. Open `http://<esp32-ip>/ota` in your web browser.
3. Select or drop `HVAC_NanoSensorHub/.pio/build/minicore_328p/firmware.hex`. The UI will auto-detect target as **Arduino Nano Sensor Hub**.
4. Click **Upload & Flash**. The ESP32 sends `<B>` over UART to reset the Nano into `urboot`, streams the 128-byte flash pages via `urprotocol`, and restarts the sensor hub in ~12 seconds.
