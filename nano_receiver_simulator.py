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
current_pm1_0 = 5.2
current_pm2_5 = 8.9
current_pm4_0 = 10.1
current_pm10_0 = 12.5
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

def format_packet(p, pulse_count, t, h, co2, voc, nox, amps, pm1, pm25, pm4, pm10):
    """Formats the data into the <pressure,pulse_count,temp,humi,co2,voc,nox,amps,pm...,checksum>\n protocol."""
    p_val, t_val, h_val = int(p * 10), int(t * 10), int(h * 10)
    co2_val, amps_val = int(co2), int(amps * 100)
    pm1_val, pm25_val = int(pm1 * 10), int(pm25 * 10)
    pm4_val, pm10_val = int(pm4 * 10), int(pm10 * 10)
    payload = f"{p_val},{pulse_count},{t_val},{h_val},{co2_val},{int(voc)},{int(nox)},{amps_val},{pm1_val},{pm25_val},{pm4_val},{pm10_val}"
    data_part = f"S{payload}"
    checksum = calculate_checksum(data_part)
    return f"<{data_part},{checksum}>\n"

def user_input_thread():
    """A separate thread to handle user input without blocking."""
    global current_pressure, current_pulse_count, current_temperature, current_humidity
    global current_co2, current_voc_raw, current_nox_raw, current_amps, stop_threads, ser
    global current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
    
    print("\n--- Interactive Simulator Control ---")
    # ... (omitting the print block for brevity, it's unchanged) ...
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
                print(f"--> [SIM SENDING CMD]: {packet.strip()}")
                if ser and ser.is_open:
                    ser.write(packet.encode('ascii'))
                continue # Skip the rest of the parsing
            # --- END OF CORRECTION ---

            parts = command.split()
            cmd_type = parts[0].lower()

            if cmd_type == 'amps' and len(parts) > 1:
                current_amps = float(parts[1])
                print(f"--> [SIM] Current set to {current_amps:.2f} A")
            # ... other sim-only commands (temp, humi, etc.) ...
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
                    handle_command_packet(ser, line)
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

def handle_command_packet(ser, packet_str):
    """Handle command packets from the Nano (which are responses or sensor data)."""
    try:
        packet_content = packet_str[1:-1]
        last_comma_idx = packet_content.rfind(',')
        if last_comma_idx == -1: return

        data_part = packet_content[:last_comma_idx]
        received_checksum = int(packet_content[last_comma_idx + 1:])
        
        calculated_checksum = calculate_checksum(data_part)
        if calculated_checksum != received_checksum:
            print(f"--> [SIM-WARNING] Checksum mismatch! Packet: {packet_str}")
            return
            
        print(f"\n<-- [NANO RESPONSE RECEIVED] Packet: {packet_str.strip()}")

    except Exception as e:
        print(f"--> [SIM-ERROR] Error handling packet: {e} | Packet: {packet_str}")

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