import serial
import time
import threading
import sys

# --- Configuration ---
SEND_INTERVAL_SECONDS = 2

# --- Global variables ---
current_pressure = 50.0
current_pulse_count = 1
current_temperature = 21.5
current_humidity = 45.0
current_co2 = 450
current_voc_raw = 27500
current_nox_raw = 15000
current_amps = 0.0
current_compressor_amps = 8.5
current_geothermal_pump_amps = 2.1
current_liquid_level_sensor_state = 1
current_pm1_0 = 5.2
current_pm2_5 = 8.9
current_pm4_0 = 10.1
current_pm10_0 = 12.5

# I2C Bridge Sensor Data
current_aht20_temp = 22.3  # AHT20 temperature (°C)
current_aht20_humidity = 48.7  # AHT20 humidity (%)
current_bmp280_pressure = 101325.0  # BMP280 pressure (Pa)
current_bmp280_temp = 21.8  # BMP280 temperature (°C)
current_zmod4510_o3 = 45  # ZMOD4510 O3 concentration (ppb)
current_zmod4510_no2 = 12  # ZMOD4510 NO2 concentration (ppb)
current_zmod4510_fast_aqi = 25  # ZMOD4510 Fast AQI
current_zmod4510_epa_aqi = 30  # ZMOD4510 EPA AQI

# I2C Bridge Sensor Addresses
AHT20_ADDRESS = 0x38
BMP280_ADDRESS = 0x77  # Could also be 0x76
ZMOD4510_ADDRESS = 0x32

stop_threads = False
send_sensor_data = True
ser = None # Make serial object global

# Using CRC-8 with polynomial 0x07 (x^8 + x^2 + x^1 + x^0)
def calculate_checksum(data_str):
    """Calculates the CRC-8 checksum for a given string (polynomial 0x07)."""
    crc = 0x00
    polynomial = 0x07
    for byte_char in data_str.encode('ascii'):
        crc ^= byte_char
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ polynomial
            else:
                crc <<= 1
        crc &= 0xFF
    return crc

def format_packet(p, pulse_count, t, h, co2, voc, nox, amps, pm1, pm25, pm4, pm10, compressor_amps, geothermal_pump_amps, liquid_level_sensor_state):
    """Formats the data into the <pressure,pulse_count,temp,humi,co2,voc,nox,amps,pm...,compressor_amps,geothermal_pump_amps,liquid_level_sensor_state,checksum>\n protocol."""
    p_val, t_val, h_val = int(p * 10), int(t * 10), int(h * 10)
    co2_val, amps_val = int(co2), int(amps * 100)
    pm1_val, pm25_val = int(pm1 * 10), int(pm25 * 10)
    pm4_val, pm10_val = int(pm4 * 10), int(pm10 * 10)
    compressor_amps_val = int(compressor_amps * 100)
    geothermal_pump_amps_val = int(geothermal_pump_amps * 100)
    payload = f"{p_val},{pulse_count},{t_val},{h_val},{co2_val},{int(voc)},{int(nox)},{amps_val},{pm1_val},{pm25_val},{pm4_val},{pm10_val},{compressor_amps_val},{geothermal_pump_amps_val},{int(liquid_level_sensor_state)}"
    data_part = f"S{payload}"
    checksum = calculate_checksum(data_part)
    return f"<{data_part},{checksum}>\n"

def send_i2c_read_command(ser, address, num_bytes):
    """Send I2C read command to Arduino Nano (acting as ESP32)."""
    print(f"[ESP32] Sending I2C read: address=0x{address:02X}, bytes={num_bytes}")
    
    # Format I2C read command: I<address>,<bytes>
    command = f"I{address:02X},{num_bytes:02X}"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    ser.write(packet.encode())
    print(f"[ESP32] Sent read command: {packet.strip()}")

def send_i2c_write_command(ser, address, data):
    """Send I2C write command to Arduino Nano (acting as ESP32)."""
    print(f"[ESP32] Sending I2C write: address=0x{address:02X}, data={[f'0x{b:02X}' for b in data]}")
    
    # Format I2C write command: W<address>,<len>,<data...>
    data_hex = ','.join([f"{b:02X}" for b in data])
    command = f"W{address:02X},{len(data):02X},{data_hex}"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    ser.write(packet.encode())
    print(f"[ESP32] Sent write command: {packet.strip()}")

def handle_nano_response(packet_str):
    """Handle response from Arduino Nano."""
    try:
        # Remove < > and split by comma
        data_part = packet_str.strip('<>').split(',')
        if len(data_part) < 2:
            return
        
        cmd = data_part[0]
        
        if cmd.startswith('i'):  # I2C Read Response
            if len(data_part) >= 3:
                address = int(data_part[0][1:], 16)
                data_len = int(data_part[1], 16)
                data_bytes = []
                for i in range(2, min(2 + data_len, len(data_part))):
                    data_bytes.append(int(data_part[i], 16))
                print(f"[NANO] I2C read response: address=0x{address:02X}, data={[f'0x{b:02X}' for b in data_bytes]}")
        elif cmd.startswith('w'):  # I2C Write Response
            if len(data_part) >= 2:
                address = int(data_part[0][1:], 16)
                status = int(data_part[1], 16)
                print(f"[NANO] I2C write response: address=0x{address:02X}, status=0x{status:02X}")
        elif cmd.startswith('v'):  # Version Response
            print(f"[NANO] Version response: {data_part}")
        elif cmd.startswith('h'):  # Health Response
            print(f"[NANO] Health response: {data_part}")
        else:
            print(f"[NANO] Unknown response: {packet_str.strip()}")
            
    except Exception as e:
        print(f"[ESP32] Error handling Nano response: {e}")

def user_input_thread():
    """A separate thread to handle user input without blocking."""
    global current_pressure, current_pulse_count, current_temperature, current_humidity
    global current_co2, current_voc_raw, current_nox_raw, current_amps, stop_threads, ser
    global current_compressor_amps, current_geothermal_pump_amps, current_liquid_level_sensor_state
    global current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
    global current_aht20_temp, current_aht20_humidity, current_bmp280_pressure, current_bmp280_temp
    global current_zmod4510_o3, current_zmod4510_no2, current_zmod4510_fast_aqi, current_zmod4510_epa_aqi
    
    print("\n--- ESP32 Simulator Control ---")
    print("Commands:")
    print("  V                 - Request version from Nano")
    print("  H                 - Request health status from Nano")
    print("  A                 - Acknowledge health status")
    print("  read_aht20        - Send I2C read to AHT20 (7 bytes)")
    print("  read_bmp280       - Send I2C read to BMP280 (6 bytes)")
    print("  read_zmod4510     - Send I2C read to ZMOD4510 (4 bytes)")
    print("  write_aht20       - Send I2C write to AHT20 (trigger measurement)")
    print("  write_bmp280      - Send I2C write to BMP280 (trigger measurement)")
    print("  write_zmod4510    - Send I2C write to ZMOD4510 (trigger measurement)")
    print("  I<addr>,<bytes>   - Send custom I2C read (e.g., I38,07)")
    print("  W<addr>,<len>,<data> - Send custom I2C write (e.g., W38,03,AC,33,00)")
    print("  compressor_amps <number> - Set compressor current in Amps (e.g., compressor_amps 8.5)")
    print("  geothermal_amps <number> - Set geothermal pump current in Amps (e.g., geothermal_amps 2.1)")
    print("  liquid_level <0|1> - Set liquid level sensor state (e.g., liquid_level 1)")
    print("  zmod4510_init     - Send initialization sequence to ZMOD4510")
    print("  quit              - Exit the simulator")
    print("-------------------------------------\n")
    
    while not stop_threads:
        try:
            command = input()
            if not command:
                continue
            if command.lower() == 'quit':
                stop_threads = True
                break
            
            # --- THIS IS THE CORRECTED LOGIC ---
            # Check if it's a command that needs to be sent to the Nano
            if command[0].upper() in ['I', 'V', 'H', 'A', 'R', 'P', 'C', 'G', 'D', 'T', 'F', 'W']:
                # The command typed by the user IS the data payload
                data_part = command
                checksum = calculate_checksum(data_part)
                packet = f"<{data_part},{checksum}>\n"
                print(f"--> [ESP32 SENDING CMD]: {packet.strip()}")
                if ser and ser.is_open:
                    ser.write(packet.encode('ascii'))
                continue # Skip the rest of the parsing
            # --- END OF CORRECTION ---

            parts = command.split()
            cmd_type = parts[0].lower()

            if cmd_type == 'read_aht20' and len(parts) == 1:
                send_i2c_read_command(ser, AHT20_ADDRESS, 7)
            elif cmd_type == 'read_bmp280' and len(parts) == 1:
                send_i2c_read_command(ser, BMP280_ADDRESS, 6)
            elif cmd_type == 'read_zmod4510' and len(parts) == 1:
                send_i2c_read_command(ser, ZMOD4510_ADDRESS, 4)
            elif cmd_type == 'write_aht20' and len(parts) == 1:
                send_i2c_write_command(ser, AHT20_ADDRESS, [0xAC, 0x33, 0x00]) # Trigger measurement
            elif cmd_type == 'write_bmp280' and len(parts) == 1:
                send_i2c_write_command(ser, BMP280_ADDRESS, [0xF4, 0x2E]) # Trigger measurement
            elif cmd_type == 'write_zmod4510' and len(parts) == 1:
                send_i2c_write_command(ser, ZMOD4510_ADDRESS, [0x00, 0x00, 0x00, 0x00]) # Trigger measurement
            elif cmd_type == 'amps' and len(parts) > 1:
                current_amps = float(parts[1])
                print(f"--> [SIM] Current set to {current_amps:.2f} A")
            elif cmd_type == 'pulse' and len(parts) > 1:
                current_pulse_count = int(parts[1])
                print(f"--> [SIM] Pulse count set to {current_pulse_count}")
            elif cmd_type == 'temp' and len(parts) > 1:
                current_temperature = float(parts[1])
                print(f"--> [SIM] Temperature set to {current_temperature:.1f} C")
            elif cmd_type == 'humi' and len(parts) > 1:
                current_humidity = float(parts[1])
                print(f"--> [SIM] Humidity set to {current_humidity:.1f} %")
            elif cmd_type == 'co2' and len(parts) > 1:
                current_co2 = int(parts[1])
                print(f"--> [SIM] CO2 set to {current_co2} ppm")
            elif cmd_type == 'voc' and len(parts) > 1:
                current_voc_raw = int(parts[1])
                print(f"--> [SIM] VOC raw value set to {current_voc_raw}")
            elif cmd_type == 'nox' and len(parts) > 1:
                current_nox_raw = int(parts[1])
                print(f"--> [SIM] NOx raw value set to {current_nox_raw}")
            elif cmd_type == 'pm1' and len(parts) > 1:
                current_pm1_0 = float(parts[1])
                print(f"--> [SIM] PM1.0 set to {current_pm1_0:.1f} µg/m³")
            elif cmd_type == 'pm25' and len(parts) > 1:
                current_pm2_5 = float(parts[1])
                print(f"--> [SIM] PM2.5 set to {current_pm2_5:.1f} µg/m³")
            elif cmd_type == 'pm4' and len(parts) > 1:
                current_pm4_0 = float(parts[1])
                print(f"--> [SIM] PM4.0 set to {current_pm4_0:.1f} µg/m³")
            elif cmd_type == 'pm10' and len(parts) > 1:
                current_pm10_0 = float(parts[1])
                print(f"--> [SIM] PM10.0 set to {current_pm10_0:.1f} µg/m³")
            elif cmd_type == 'aht20_temp' and len(parts) > 1:
                current_aht20_temp = float(parts[1])
                print(f"--> [SIM] AHT20 Temperature set to {current_aht20_temp:.1f} C")
            elif cmd_type == 'aht20_humi' and len(parts) > 1:
                current_aht20_humidity = float(parts[1])
                print(f"--> [SIM] AHT20 Humidity set to {current_aht20_humidity:.1f} %")
            elif cmd_type == 'bmp280_pressure' and len(parts) > 1:
                current_bmp280_pressure = float(parts[1])
                print(f"--> [SIM] BMP280 Pressure set to {current_bmp280_pressure:.0f} Pa")
            elif cmd_type == 'bmp280_temp' and len(parts) > 1:
                current_bmp280_temp = float(parts[1])
                print(f"--> [SIM] BMP280 Temperature set to {current_bmp280_temp:.1f} C")
            elif cmd_type == 'zmod4510_o3' and len(parts) > 1:
                current_zmod4510_o3 = int(parts[1])
                print(f"--> [SIM] ZMOD4510 O3 set to {current_zmod4510_o3} ppb")
            elif cmd_type == 'zmod4510_no2' and len(parts) > 1:
                current_zmod4510_no2 = int(parts[1])
                print(f"--> [SIM] ZMOD4510 NO2 set to {current_zmod4510_no2} ppb")
            elif cmd_type == 'zmod4510_fast_aqi' and len(parts) > 1:
                current_zmod4510_fast_aqi = int(parts[1])
                print(f"--> [SIM] ZMOD4510 Fast AQI set to {current_zmod4510_fast_aqi}")
            elif cmd_type == 'zmod4510_epa_aqi' and len(parts) > 1:
                current_zmod4510_epa_aqi = int(parts[1])
                print(f"--> [SIM] ZMOD4510 EPA AQI set to {current_zmod4510_epa_aqi}")
            elif cmd_type == 'compressor_amps' and len(parts) > 1:
                current_compressor_amps = float(parts[1])
                print(f"--> [SIM] Compressor current set to {current_compressor_amps:.2f} A")
            elif cmd_type == 'geothermal_amps' and len(parts) > 1:
                current_geothermal_pump_amps = float(parts[1])
                print(f"--> [SIM] Geothermal pump current set to {current_geothermal_pump_amps:.2f} A")
            elif cmd_type == 'liquid_level' and len(parts) > 1:
                current_liquid_level_sensor_state = int(parts[1])
                print(f"--> [SIM] Liquid level sensor state set to {current_liquid_level_sensor_state}")
            elif cmd_type == 'zmod4510_init':
                zmod4510_init_sequence(ser)
                print("--> [SIM] ZMOD4510 init sequence sent.")
            else:
                current_pressure = float(command)
                print(f"--> [SIM] Pressure set to {current_pressure:.1f} Pa")

        except (ValueError, IndexError):
            print("--> [SIM-ERROR] Invalid input. Please use the correct format.")
        except (EOFError, KeyboardInterrupt):
            stop_threads = True
            break

def serial_reader_thread(ser):
    """A thread to continuously read and print data from the serial port."""
    global stop_threads
    while not stop_threads:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                if line.startswith('<') and line.endswith('>'):
                    handle_nano_response(line)
                else:
                    # It's not a packet, so treat it as a debug message from the Nano
                    print(f"[NANO MSG]: {line}")
        except serial.SerialException:
            print("--> [SIM-ERROR] Serial port disconnected.")
            stop_threads = True
            break
        except Exception as e:
            print(f"--> [SIM-ERROR] An error occurred in reader thread: {e}")
            stop_threads = True
            break
        time.sleep(0.01)

def zmod4510_init_sequence(ser):
    """Simulate the ZMOD4510 initialization sequence as seen in the logs."""
    # All addresses are 0x33 (hex), which is 51 decimal
    addr = 0x33

    # 1. W33,00
    send_i2c_write_command(ser, addr, [])

    # 2. W33,02,93,00
    send_i2c_write_command(ser, addr, [0x93, 0x00])

    # 3. I33,01,01,94 (write 94, read 1)
    # Custom: I<addr>,<writeLen>,<readLen>,<writeData...>
    command = f"I{addr:02X},01,01,94"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 4. I33,01,02,00 (write 00, read 2)
    command = f"I{addr:02X},01,02,00"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 5. I33,01,06,20 (write 20, read 6)
    command = f"I{addr:02X},01,06,20"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 6. I33,01,0A,26 (write 26, read 10)
    command = f"I{addr:02X},01,0A,26"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 7. I33,01,06,3A (write 3A, read 6)
    command = f"I{addr:02X},01,06,3A"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 8. I33,01,01,B7 (write B7, read 1)
    command = f"I{addr:02X},01,01,B7"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 9. W33,03,40,01,FC
    send_i2c_write_command(ser, addr, [0x40, 0x01, 0xFC])

    # 10. W33,03,50,00,28
    send_i2c_write_command(ser, addr, [0x50, 0x00, 0x28])

    # 11. W33,03,60,C3,E3
    send_i2c_write_command(ser, addr, [0x60, 0xC3, 0xE3])

    # 12. W33,05,68,00,00,80,40
    send_i2c_write_command(ser, addr, [0x68, 0x00, 0x00, 0x80, 0x40])

    # 13. W33,02,93,80
    send_i2c_write_command(ser, addr, [0x93, 0x80])

    # 14. I33,01,01,94 (write 94, read 1)
    command = f"I{addr:02X},01,01,94"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 15. I33,01,04,97 (write 97, read 4)
    command = f"I{addr:02X},01,04,97"
    checksum = calculate_checksum(command)
    packet = f"<{command},{checksum}>\n"
    print(f"[ESP32] Sending I2C write-read: {packet.strip()}")
    if ser and ser.is_open:
        ser.write(packet.encode('ascii'))

    # 16. W33,09,40,01,FC,03,18,03,6E,03,C4
    send_i2c_write_command(ser, addr, [0x40, 0x01, 0xFC, 0x03, 0x18, 0x03, 0x6E, 0x03, 0xC4])

    # 17. W33,09,50,00,10,00,52,3F,66,00,42
    send_i2c_write_command(ser, addr, [0x50, 0x00, 0x10, 0x00, 0x52, 0x3F, 0x66, 0x00, 0x42])

    # 18. W33,03,60,23,03
    send_i2c_write_command(ser, addr, [0x60, 0x23, 0x03])

    # 19. W33,21,68,00,00,02,41,00,41,00,41,00,49,00,50,02,42,00,42,00,42,00,4A,00,50,02,43,00,43,00,43,00,43,80,5B
    send_i2c_write_command(ser, addr, [
        0x68, 0x00, 0x00, 0x02, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x49, 0x00, 0x50, 0x02, 0x42, 0x00,
        0x42, 0x00, 0x42, 0x00, 0x4A, 0x00, 0x50, 0x02, 0x43, 0x00, 0x43, 0x00, 0x43, 0x00, 0x43, 0x80, 0x5B
    ])

def main():
    """Main function to run the simulator."""
    global stop_threads, send_sensor_data, ser

    if len(sys.argv) < 2:
        print("Usage: python simulator.py <COM_PORT> [--no-sensor-packets]")
        sys.exit(1)

    port_name = sys.argv[1]
    
    if "--no-sensor-packets" in sys.argv:
        send_sensor_data = False
        print("--> [SIM-INFO] Periodic sensor data packets are DISABLED.")
    
    try:
        ser = serial.Serial(port_name, 19200, timeout=1)
        print(f"Successfully opened serial port {port_name} at 19200 baud.")
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {port_name}.")
        print(e)
        sys.exit(1)
        
    input_handler = threading.Thread(target=user_input_thread, daemon=True)
    reader_handler = threading.Thread(target=serial_reader_thread, args=(ser,), daemon=True)
    input_handler.start()
    reader_handler.start()

    # The main loop now only needs to send sensor packets if enabled
    try:
        while not stop_threads:
            if send_sensor_data:
                 # This functionality is for simulating the main sensor board, not the ESP32.
                 # For now, we will disable it to focus on command-response.
                 pass
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nExiting simulator.")
    finally:
        stop_threads = True
        time.sleep(0.1)
        if 'ser' in globals() and ser is not None and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()