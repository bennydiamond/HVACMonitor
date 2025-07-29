import serial
import time
import threading
import sys

# --- Configuration ---
SEND_INTERVAL_SECONDS = 2

# --- Global variables ---
current_pressure = 50.0
current_pulse_count = 1  # Number of pulses since last packet
current_temperature = 21.5
current_humidity = 45.0
current_co2 = 450
current_voc_raw = 27500  # A typical raw VOC value
current_nox_raw = 15000  # A typical raw NOx value for SGP41
current_amps = 0.0 # <-- New variable for amps
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
serial_buffer = ""

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
        crc &= 0xFF # Ensure it stays 8-bit

    return crc

def format_packet(p, pulse_count, t, h, co2, voc, nox, amps, pm1, pm25, pm4, pm10, 
                 aht20_temp, aht20_humidity, bmp280_pressure, bmp280_temp, 
                 zmod4510_o3, zmod4510_no2, zmod4510_fast_aqi, zmod4510_epa_aqi):
    """Formats the data into the <pressure,pulse_count,temp,humi,co2,voc,nox,amps,pm...,aht20_temp,aht20_humidity,bmp280_pressure,bmp280_temp,zmod4510_o3,zmod4510_no2,zmod4510_fast_aqi,zmod4510_epa_aqi,checksum>\n protocol."""
    # Scale float values to integers to match the Arduino Nano's new format
    p_val = int(p * 10)
    t_val = int(t * 10)
    h_val = int(h * 10)
    co2_val = int(co2)
    amps_val = int(amps * 100)
    pm1_val = int(pm1 * 10)
    pm25_val = int(pm25 * 10)
    pm4_val = int(pm4 * 10)
    pm10_val = int(pm10 * 10)
    
    # I2C Bridge sensor values (scaled to integers)
    aht20_temp_val = int(aht20_temp * 10)
    aht20_humidity_val = int(aht20_humidity * 10)
    bmp280_pressure_val = int(bmp280_pressure / 10)  # Pressure in Pa, scale down
    bmp280_temp_val = int(bmp280_temp * 10)
    zmod4510_o3_val = int(zmod4510_o3)
    zmod4510_no2_val = int(zmod4510_no2)
    zmod4510_fast_aqi_val = int(zmod4510_fast_aqi)
    zmod4510_epa_aqi_val = int(zmod4510_epa_aqi)

    # Format the data part with scaled integers
    payload = f"{p_val},{pulse_count},{t_val},{h_val},{co2_val},{int(voc)},{int(nox)},{amps_val},{pm1_val},{pm25_val},{pm4_val},{pm10_val},{aht20_temp_val},{aht20_humidity_val},{bmp280_pressure_val},{bmp280_temp_val},{zmod4510_o3_val},{zmod4510_no2_val},{zmod4510_fast_aqi_val},{zmod4510_epa_aqi_val}"
    data_part = f"S{payload}"
    checksum = calculate_checksum(data_part)
    return f"<{data_part},{checksum}>\n"

def user_input_thread():
    """A separate thread to handle user input without blocking."""
    global current_pressure, current_pulse_count, current_temperature, current_humidity
    global current_co2, current_voc_raw, current_nox_raw, current_amps, stop_threads
    global current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
    global current_aht20_temp, current_aht20_humidity, current_bmp280_pressure, current_bmp280_temp
    global current_zmod4510_o3, current_zmod4510_no2, current_zmod4510_fast_aqi, current_zmod4510_epa_aqi
    
    print("\n--- Interactive Simulator Control ---")
    print("Commands:")
    print("  <number>        - Set pressure (e.g., 150.5)")
    print("  amps <number>   - Set AC Current in Amps (e.g., amps 1.5)")
    print("  pulse <number>  - Set pulse count (e.g., pulse 2)")
    print("  temp <number>   - Set temperature (e.g., temp 23.8)")
    print("  humi <number>   - Set humidity (e.g., humi 55.2)")
    print("  co2 <number>    - Set CO2 in ppm (e.g., co2 800)")
    print("  voc <number>    - Set raw VOC value (e.g., voc 28000)")
    print("  nox <number>    - Set raw NOx value (e.g., nox 15000)")
    print("  pm1 <number>    - Set PM1.0 (e.g., pm1 5.2)")
    print("  pm25 <number>   - Set PM2.5 (e.g., pm25 9.8)")
    print("  pm4 <number>    - Set PM4.0 (e.g., pm4 11.3)")
    print("  pm10 <number>   - Set PM10.0 (e.g., pm10 15.1)")
    print("  aht20_temp <number> - Set AHT20 temperature (e.g., aht20_temp 24.5)")
    print("  aht20_humi <number> - Set AHT20 humidity (e.g., aht20_humi 52.3)")
    print("  bmp280_pressure <number> - Set BMP280 pressure in Pa (e.g., bmp280_pressure 101325)")
    print("  bmp280_temp <number> - Set BMP280 temperature (e.g., bmp280_temp 22.1)")
    print("  zmod4510_o3 <number> - Set ZMOD4510 O3 in ppb (e.g., zmod4510_o3 45)")
    print("  zmod4510_no2 <number> - Set ZMOD4510 NO2 in ppb (e.g., zmod4510_no2 12)")
    print("  zmod4510_fast_aqi <number> - Set ZMOD4510 Fast AQI (e.g., zmod4510_fast_aqi 25)")
    print("  zmod4510_epa_aqi <number> - Set ZMOD4510 EPA AQI (e.g., zmod4510_epa_aqi 30)")
    print("  quit            - Exit the simulator")
    print("-------------------------------------\n")
    
    while not stop_threads:
        try:
            command = input()
            if command.lower() == 'quit':
                stop_threads = True
                break
            
            parts = command.split()
            cmd_type = parts[0].lower()

            if cmd_type == 'amps' and len(parts) > 1:
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
                print(f"--> [SIM] Raw VOC set to {current_voc_raw}")
            elif cmd_type == 'nox' and len(parts) > 1:
                current_nox_raw = int(parts[1])
                print(f"--> [SIM] Raw NOx set to {current_nox_raw}")
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
            else:
                current_pressure = float(command)
                print(f"--> [SIM] Pressure set to {current_pressure:.1f} Pa")

        except (ValueError, IndexError):
            print("--> [SIM-ERROR] Invalid input. Please use the correct format.")
        except (EOFError, KeyboardInterrupt):
            stop_threads = True
            break

def serial_reader_thread(ser):
    """
    A thread to continuously read data, printing raw/crash data while
    also processing standard command packets.
    """
    global stop_threads, serial_buffer

    while not stop_threads:
        try:
            # Read all available data and append to the buffer
            if ser.in_waiting > 0:
                # Decode using utf-8, ignoring errors which are common in crash dumps
                serial_buffer += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')

            # Process the buffer for both raw data and structured packets
            while True:
                start_idx = serial_buffer.find('<')
                
                if start_idx == -1:
                    # No packet start found, so the entire buffer is raw data
                    if len(serial_buffer) > 0:
                        print(f"\n[ESP32-RAW]: {serial_buffer.strip()}")
                        serial_buffer = "" # Clear buffer after printing
                    break # Wait for more data
                
                # Check for and print any raw data before a packet starts
                if start_idx > 0:
                    raw_data = serial_buffer[:start_idx]
                    print(f"\n[ESP32-RAW]: {raw_data.strip()}")
                    serial_buffer = serial_buffer[start_idx:] # Trim the raw data
                    # After trimming, re-find the start index which is now 0
                    start_idx = 0 
                
                # Now, look for a complete packet at the start of the buffer
                end_idx = serial_buffer.find('>')
                if start_idx == 0 and end_idx > start_idx:
                    # A complete packet is found
                    packet = serial_buffer[start_idx : end_idx + 1]
                    handle_command_packet(ser, packet) # Process it
                    
                    # Remove the processed packet from the buffer
                    serial_buffer = serial_buffer[end_idx + 1:]
                else:
                    # Incomplete packet (< found, but no > yet), wait for more data
                    break

        except serial.SerialException:
            print("--> [SIM-ERROR] Serial port disconnected.")
            stop_threads = True
            break
        except Exception as e:
            print(f"--> [SIM-ERROR] An error occurred in reader thread: {e}")
            stop_threads = True
            break

        time.sleep(0.01) # Sleep briefly to prevent a busy-wait loop


def handle_command_packet(ser, packet_str):
    """Handle command packets from the ESP32."""
    print(f"[CMD] Received: {packet_str.strip()}")
    
    # Check if this is an I2C bridge command
    cmd_type, address, param1, param2 = parse_i2c_command(packet_str)
    if cmd_type == 'read':
        handle_i2c_read_request(ser, address, param1)
        return
    elif cmd_type == 'write':
        handle_i2c_write_request(ser, address, param2)
        return
    
    # Handle other commands as before
    try:
        # Remove < > and split by comma
        data_part = packet_str.strip('<>').split(',')
        if len(data_part) < 2:
            return
        
        cmd = data_part[0]
        
        if cmd.startswith('V'):  # Version request
            response = "v1.0.0,I2C_Bridge_Simulator"
            checksum = calculate_checksum(response)
            packet = f"<{response},{checksum}>\n"
            ser.write(packet.encode())
            print(f"[CMD] Sent version response: {packet.strip()}")
            
        elif cmd.startswith('H'):  # Health request
            response = "h1,1,1,1,1,1"  # All sensors healthy
            checksum = calculate_checksum(response)
            packet = f"<{response},{checksum}>\n"
            ser.write(packet.encode())
            print(f"[CMD] Sent health response: {packet.strip()}")
            
        elif cmd.startswith('A'):  # ACK Health
            print("[CMD] Acknowledged health status")
            
        else:
            print(f"[CMD] Unknown command: {cmd}")
            
    except Exception as e:
        print(f"[CMD] Error handling command: {e}")

def handle_i2c_read_request(ser, address, num_bytes):
    """Handle I2C read request and respond with dummy sensor data."""
    print(f"[I2C] Read request: address=0x{address:02X}, bytes={num_bytes}")
    
    # Generate dummy data based on sensor address
    if address == AHT20_ADDRESS:
        # AHT20 data: status byte + 6 bytes of temperature/humidity data
        if num_bytes >= 7:
            # Status byte (not busy, calibrated)
            status = 0x08  # Calibrated bit set
            # Temperature data (24-bit, scaled)
            temp_raw = int(current_aht20_temp * 1048576 / 200 + 1048576)  # Convert to AHT20 format
            # Humidity data (24-bit, scaled)
            hum_raw = int(current_aht20_humidity * 1048576 / 100)
            
            data = [status, 
                   (hum_raw >> 16) & 0xFF, (hum_raw >> 8) & 0xFF, hum_raw & 0xFF,
                   (temp_raw >> 16) & 0xFF, (temp_raw >> 8) & 0xFF, temp_raw & 0xFF]
            print(f"[I2C] AHT20 response: temp={current_aht20_temp:.1f}°C, hum={current_aht20_humidity:.1f}%")
        else:
            data = [0x08]  # Just status byte
    elif address == BMP280_ADDRESS:
        # BMP280 data: pressure and temperature registers
        if num_bytes >= 6:
            # Pressure data (24-bit)
            pressure_raw = int(current_bmp280_pressure * 256 / 100000)  # Scale to BMP280 format
            # Temperature data (20-bit)
            temp_raw = int((current_bmp280_temp + 50) * 512)  # Convert to BMP280 format
            
            data = [(pressure_raw >> 16) & 0xFF, (pressure_raw >> 8) & 0xFF, pressure_raw & 0xFF,
                   (temp_raw >> 12) & 0xFF, (temp_raw >> 4) & 0xFF, (temp_raw << 4) & 0xFF]
            print(f"[I2C] BMP280 response: pressure={current_bmp280_pressure:.0f}Pa, temp={current_bmp280_temp:.1f}°C")
        else:
            data = [0x00]  # Default response
    elif address == ZMOD4510_ADDRESS:
        # ZMOD4510 data: measurement results
        if num_bytes >= 4:
            # Simplified ZMOD4510 response (O3 and NO2 concentrations)
            data = [current_zmod4510_o3 & 0xFF, (current_zmod4510_o3 >> 8) & 0xFF,
                   current_zmod4510_no2 & 0xFF, (current_zmod4510_no2 >> 8) & 0xFF]
            print(f"[I2C] ZMOD4510 response: O3={current_zmod4510_o3}ppb, NO2={current_zmod4510_no2}ppb")
        else:
            data = [0x00]  # Default response
    else:
        # Unknown sensor, return zeros
        data = [0x00] * min(num_bytes, 32)
        print(f"[I2C] Unknown sensor address 0x{address:02X}, returning zeros")
    
    # Limit data length
    data = data[:min(num_bytes, 32)]
    
    # Send I2C read response
    data_hex = ','.join([f"{b:02X}" for b in data])
    response = f"i{address:02X},{len(data):02X},{data_hex}"
    checksum = calculate_checksum(response)
    packet = f"<{response},{checksum}>\n"
    ser.write(packet.encode())
    print(f"[I2C] Sent read response: {packet.strip()}")

def handle_i2c_write_request(ser, address, data):
    """Handle I2C write request."""
    print(f"[I2C] Write request: address=0x{address:02X}, data={[f'0x{b:02X}' for b in data]}")
    
    # Send I2C write response (success)
    response = f"w{address:02X},00"  # 00 = no error
    checksum = calculate_checksum(response)
    packet = f"<{response},{checksum}>\n"
    ser.write(packet.encode())
    print(f"[I2C] Sent write response: {packet.strip()}")

def parse_i2c_command(packet_str):
    """Parse I2C bridge command from packet string."""
    try:
        # Remove < > and split by comma
        data_part = packet_str.strip('<>').split(',')
        if len(data_part) < 2:
            return None, None, None, None
        
        cmd = data_part[0]
        if cmd.startswith('I'):  # I2C Read
            if len(data_part) >= 3:
                address = int(data_part[0][1:], 16)
                num_bytes = int(data_part[1], 16)
                return 'read', address, num_bytes, None
        elif cmd.startswith('W'):  # I2C Write
            if len(data_part) >= 3:
                address = int(data_part[0][1:], 16)
                data_len = int(data_part[1], 16)
                data = []
                for i in range(2, min(2 + data_len, len(data_part))):
                    data.append(int(data_part[i], 16))
                return 'write', address, data_len, data
        
        return None, None, None, None
    except (ValueError, IndexError) as e:
        print(f"[I2C] Error parsing command: {e}")
        return None, None, None, None


def main():
    """Main function to run the simulator."""
    global stop_threads
    
    port_name = ""
    if len(sys.argv) > 1:
        port_name = sys.argv[1]
    else:
        port_name = input("Enter the COM port of your USB-to-Serial adapter (e.g., COM3 or /dev/ttyUSB0): ")
    
    try:
        # MODIFIED: Changed baud rate to 115200 to capture ESP32 crash logs
        ser = serial.Serial(port_name, 115200, timeout=1)
        print(f"Successfully opened serial port {port_name} at 115200 baud.")
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {port_name}.")
        print(e)
        sys.exit(1)
        
    input_handler = threading.Thread(target=user_input_thread, daemon=True)
    reader_handler = threading.Thread(target=serial_reader_thread, args=(ser,), daemon=True)
    input_handler.start()
    reader_handler.start()

    print(f"Starting data injection. A new packet will be sent every {SEND_INTERVAL_SECONDS} seconds.")

    try:
        while not stop_threads:
            # Pass all variables including NOx to the packet formatter
            packet = format_packet(
                current_pressure, current_pulse_count, current_temperature, current_humidity,
                current_co2, current_voc_raw, current_nox_raw, current_amps,
                current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0,
                current_aht20_temp, current_aht20_humidity, current_bmp280_pressure, current_bmp280_temp,
                current_zmod4510_o3, current_zmod4510_no2, current_zmod4510_fast_aqi, current_zmod4510_epa_aqi
            )
            
            print(f"--> [SIM Sending]: {packet.strip()}")
            ser.write(packet.encode('ascii'))
            
            # Sleep in small chunks to allow for quick exit
            for _ in range(SEND_INTERVAL_SECONDS * 4):
                if stop_threads:
                    break
                time.sleep(0.25)
            
    except KeyboardInterrupt:
        print("\nExiting simulator.")
    finally:
        stop_threads = True
        time.sleep(0.1)
        if ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()