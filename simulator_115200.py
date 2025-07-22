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

def format_packet(p, pulse_count, t, h, co2, voc, nox, amps, pm1, pm25, pm4, pm10):
    """Formats the data into the <pressure,pulse_count,temp,humi,co2,voc,nox,amps,pm...,checksum>\n protocol."""
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

    # Format the data part with scaled integers
    payload = f"{p_val},{pulse_count},{t_val},{h_val},{co2_val},{int(voc)},{int(nox)},{amps_val},{pm1_val},{pm25_val},{pm4_val},{pm10_val}"
    data_part = f"S{payload}"
    checksum = calculate_checksum(data_part)
    return f"<{data_part},{checksum}>\n"

def user_input_thread():
    """A separate thread to handle user input without blocking."""
    global current_pressure, current_pulse_count, current_temperature, current_humidity
    global current_co2, current_voc_raw, current_nox_raw, current_amps, stop_threads
    global current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
    
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
    try:
        # Parse the packet
        packet_str = packet_str.strip()
        if not packet_str.startswith('<') or not packet_str.endswith('>'):
            return
            
        packet_str = packet_str[1:-1]  # Remove < and >
        last_comma_idx = packet_str.rfind(',')
        if last_comma_idx == -1:
            return
            
        data_part = packet_str[:last_comma_idx]
        received_checksum = int(packet_str[last_comma_idx + 1:])
        
        # Validate checksum
        calculated_checksum = calculate_checksum(data_part)
        if calculated_checksum != received_checksum:
            print(f"--> [SIM-WARNING] Checksum mismatch in command: {packet_str}")
            return
            
        # Process the command
        cmd = data_part[0]
        payload = data_part[1:] if len(data_part) > 1 else ""
        
        print(f"--> [SIM-RECEIVED]: Command '{cmd}' with payload '{payload}'")
        
        if cmd == 'V':  # Version request
            response_data = "v1.0.0"
            checksum = calculate_checksum(response_data)
            response = f"<{response_data},{checksum}>"
            print(f"--> [SIM-SENDING]: {response}")
            ser.write(response.encode('ascii'))
            
        elif cmd == 'H':  # Health request
            response_data = "h1,512,1"
            checksum = calculate_checksum(response_data)
            response = f"<{response_data},{checksum}>"
            print(f"--> [SIM-SENDING]: {response}")
            ser.write(response.encode('ascii'))
            
        elif cmd == 'I':  # I2C Read or Write-Read request
            parts = payload.split(',')
            if len(parts) >= 2:
                addr = int(parts[0], 16)
                
                # Check if this is a simple read or a write-then-read
                if len(parts) == 2:  # Simple read: I<addr>,<read_len>
                    num_bytes = int(parts[1], 16)
                    
                    # Generate mock data
                    data_bytes = [i + 0xA0 for i in range(num_bytes)]
                    data_hex = ",".join([f"{b:02X}" for b in data_bytes])
                    
                    print(f"--> [SIM-SENDING I2C READ RESPONSE]: {num_bytes} bytes")
                    
                elif len(parts) >= 3:  # Write-then-read: I<addr>,<write_len>,<read_len>,<write_data...>
                    write_len = int(parts[1], 16)
                    read_len = int(parts[2], 16)
                    
                    # Extract write data if present
                    write_data = [int(parts[3+i], 16) for i in range(write_len)] if len(parts) > 3 else []
                    
                    # Generate response data based on the write data (register address)
                    data_bytes = []
                    
                    # ZMOD4510 specific responses
                    if len(write_data) > 0:
                        if write_data[0] == 0x94 and read_len == 1:
                            data_bytes.append(0x0F)  # Response for register 0x94
                            print(f"--> [SIM-ZMOD4510]: Responding with 0x0F for register 0x94")
                        elif write_data[0] == 0x00 and read_len == 2:
                            data_bytes = [0x63, 0x20]  # Response for register 0x00
                            print(f"--> [SIM-ZMOD4510]: Responding with 0x63 0x20 for register 0x00")
                        elif write_data[0] == 0x20 and read_len == 6:
                            data_bytes = [0xD0, 0xD2, 0x39, 0xA7, 0x4D, 0x2C]  # Response for register 0x20
                            print(f"--> [SIM-ZMOD4510]: Responding with D0 D2 39 A7 4D 2C for register 0x20")
                        elif write_data[0] == 0xB7 and read_len == 1:
                            data_bytes.append(0x00)  # Response for register 0xB7
                            print(f"--> [SIM-ZMOD4510]: Responding with 0x00 for register 0xB7")
                        else:
                            # Default pattern for other registers
                            for i in range(read_len):
                                data_bytes.append(0xB0 + i)
                    else:
                        # Default pattern when no write data
                        for i in range(read_len):
                            data_bytes.append(0xB0 + i)
                    
                    data_hex = ",".join([f"{b:02X}" for b in data_bytes])
                    
                    print(f"--> [SIM-SENDING I2C WRITE-READ RESPONSE]: {read_len} bytes")
                
                # Format response
                response_bytes = read_len if len(parts) >= 3 else num_bytes
                response_data = f"i00,{response_bytes:02X},{data_hex}"
                checksum = calculate_checksum(response_data)
                response = f"<{response_data},{checksum}>"
                
                print(f"--> [SIM-SENDING]: {response}")
                ser.write(response.encode('ascii'))
                
        elif cmd == 'W':  # I2C Write request
            response_data = "w00"  # Status 0 = success
            checksum = calculate_checksum(response_data)
            response = f"<{response_data},{checksum}>"
            print(f"--> [SIM-SENDING I2C WRITE RESPONSE]: {response}")
            ser.write(response.encode('ascii'))

        elif cmd == 'P': # SPS30 Info Request
            fw_status, fw_major, fw_minor = 0, 2, 2
            interval_status, interval_secs = 0, 604800
            days_status, interval_days = 0, 7
            dev_status_status, dev_status_reg = 0, 0

            response_data = f"p{fw_status},{fw_major},{fw_minor},{interval_status},{interval_secs},{days_status},{interval_days},{dev_status_status},{dev_status_reg}"
            checksum = calculate_checksum(response_data)
            response = f"<{response_data},{checksum}>"
            print(f"--> [SIM-SENDING SPS30 INFO]: {response}")
            ser.write(response.encode('ascii'))

        elif cmd == 'D': # SCD30 Info Request
            status = 0
            measurement_interval, auto_calib_status, force_recalib_status = 2, 1, 400
            temp_offset, alt_comp = 0, 0
            fw_major, fw_minor = 3, 67

            response_data = (
                f"d{status:X},{measurement_interval:X},"
                f"{status:X},{auto_calib_status:X},"
                f"{status:X},{force_recalib_status:X},"
                f"{status:X},{temp_offset:X},"
                f"{status:X},{alt_comp:X},"
                f",{status:X},{fw_major:X},{fw_minor:X}"
            )
            checksum = calculate_checksum(response_data)
            response = f"<{response_data},{checksum}>"
            print(f"--> [SIM-SENDING SCD30 INFO]: {response}")
            ser.write(response.encode('ascii'))

    except Exception as e:
        print(f"--> [SIM-ERROR] Error handling command packet: {e}")
        return


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
                current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
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