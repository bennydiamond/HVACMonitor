import serial
import time
import threading
import sys

# CRC-8 Implementation (copied from simulator.py for consistency)
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

def parse_packet(packet_str):
    """Parses a sensor data packet from the Nano, including checksum validation."""
    packet_str = packet_str.strip()
    if not packet_str.startswith("<") or not packet_str.endswith(">"):
        print(f"--> [ERROR] Malformed packet (no start/end markers): {packet_str}")
        return None

    packet_str = packet_str[1:-1] # Remove < and >

    last_comma_idx = packet_str.rfind(',')
    if last_comma_idx == -1:
        print(f"--> [ERROR] Malformed packet (no checksum comma): {packet_str}")
        return None

    data_part_str = packet_str[:last_comma_idx]
    try:
        received_checksum = int(packet_str[last_comma_idx + 1:])
    except ValueError:
        print(f"--> [ERROR] Invalid checksum format: {packet_str}")
        return None

    calculated_checksum = calculate_checksum(data_part_str)

    if calculated_checksum != received_checksum:
        print(f"--> [WARNING] Checksum mismatch! Received: {received_checksum}, Calculated: {calculated_checksum} for data: '{data_part_str}'")
        return None

    # Checksum passed, now parse the data values
    try:
        cmd = data_part_str[0]
        payload_str = data_part_str[1:]

        if cmd == 'S': # Sensor data
            values = payload_str.split(',')
            p = float(values[0]) / 10.0
            c = int(values[1])
            t = float(values[2]) / 10.0
            h = float(values[3]) / 10.0
            co2 = float(values[4])
            voc = int(values[5])
            amps = float(values[6]) / 100.0
            pm1 = float(values[7]) / 10.0
            pm25 = float(values[8]) / 10.0
            pm4 = float(values[9]) / 10.0
            pm10 = float(values[10]) / 10.0

            return {
                "type": "Sensor Data",
                "data": {
                    "pressure_pa": p, "cpm": c, "temperature_c": t,
                    "humidity_percent": h, "co2_ppm": co2, "voc_raw": voc,
                    "current_amps": amps, "pm1_0_ugm3": pm1, "pm2_5_ugm3": pm25,
                    "pm4_0_ugm3": pm4, "pm10_0_ugm3": pm10
                }
            }
        elif cmd == 'v': # Version response
            return { "type": "Version Response", "version": payload_str }
        elif cmd == 'h': # Health response
            parts = payload_str.split(',')
            first_time_flag = int(parts[0])
            free_ram = int(parts[1])
            reset_cause = int(parts[2])
            return { "type": "Health Response", "first_time_flag": first_time_flag, "free_ram_bytes": free_ram, "reset_cause": reset_cause }
        elif cmd == 'p': # SPS30 info response
            parts = payload_str.split(',')
            return { "type": "SPS30 Info Response", "fw_ret": int(parts[0]), "fw_major": int(parts[1]), "fw_minor": int(parts[2]), 
                    "interval_ret": int(parts[3]), "interval_sec": int(parts[4]), "days_ret": int(parts[5]), 
                    "days": int(parts[6]), "status_ret": int(parts[7]), "status_reg": int(parts[8]) }
        elif cmd == 'c': # SPS30 clean response
            return { "type": "SPS30 Clean Response", "result": int(payload_str) }
        elif cmd == 'g': # SGP40 test response
            parts = payload_str.split(',')
            return { "type": "SGP40 Test Response", "sgp40_ret": int(parts[0]), "test_result": parts[1] }
        elif cmd == 'd': # SCD30 info response
            parts = payload_str.split(',')
            return { "type": "SCD30 Info Response", 
                    "interval_ret": int(parts[0], 16), "interval": int(parts[1], 16), 
                    "auto_cal_ret": int(parts[2], 16), "auto_cal": int(parts[3], 16), 
                    "forced_cal_ret": int(parts[4], 16), "forced_cal": int(parts[5], 16), 
                    "temp_offset_ret": int(parts[6], 16), "temp_offset": int(parts[7], 16), 
                    "altitude_ret": int(parts[8], 16), "altitude": int(parts[9], 16) }
        elif cmd == 't': # SCD30 AutoCalibration response
            parts = payload_str.split(',')
            return { "type": "SCD30 AutoCal Response", 
                    "set_result": int(parts[0], 16), "read_result": int(parts[1], 16), 
                    "actual_state": "ON" if int(parts[2], 16) else "OFF" }
        elif cmd == 'f': # SCD30 Force Calibration response
            parts = payload_str.split(',')
            return { "type": "SCD30 Force Cal Response", 
                    "set_result": int(parts[0], 16), "read_result": int(parts[1], 16), 
                    "actual_value": f"{int(parts[2], 16)} ppm" }

        else:
            print(f"--> [ERROR] Unknown command received: {cmd}")
            return None

    except (ValueError, IndexError) as e:
        print(f"--> [ERROR] Error parsing data values: {e} in '{data_part_str}'")
        return None

stop_threads = False

def command_sender_thread(ser):
    """A thread to send commands to the Nano based on user input."""
    global stop_threads
    print("\n--- Nano Command Sender ---")
    print("  V - Version | H - Health | A - Ack Health | R - Reboot")
    print("  P - SPS30 Info | C - SPS30 Clean | G - SGP40 Test | D - SCD30 Info")
    print("  T0 - SCD30 AutoCal OFF | T1 - SCD30 AutoCal ON")
    print("  F<ppm> - SCD30 Force Calibration (e.g., F400)")
    print("  quit - Exit")
    print("---------------------------\n")
    while not stop_threads:
        try:
            cmd_char = input().strip().upper()
            if cmd_char == 'QUIT':
                stop_threads = True
                break
            if cmd_char in ['V', 'H', 'A', 'R', 'P', 'C', 'G', 'D']:
                checksum = calculate_checksum(cmd_char)
                packet = f"<{cmd_char},{checksum}>"
                print(f"--> Sending command: {packet}")
                ser.write(packet.encode('ascii'))
            elif cmd_char in ['T0', 'T1']:
                checksum = calculate_checksum(cmd_char)
                packet = f"<{cmd_char},{checksum}>"
                print(f"--> Sending SCD30 AutoCal command: {packet}")
                ser.write(packet.encode('ascii'))
            elif cmd_char.startswith('F') and len(cmd_char) > 1:
                checksum = calculate_checksum(cmd_char)
                packet = f"<{cmd_char},{checksum}>"
                print(f"--> Sending SCD30 Force Cal command: {packet}")
                ser.write(packet.encode('ascii'))
        except (EOFError, KeyboardInterrupt):
            stop_threads = True
            break

def main():
    global stop_threads
    port_name = ""
    if len(sys.argv) > 1:
        port_name = sys.argv[1]
    else:
        port_name = input("Enter the COM port of your Arduino Nano (e.g., COM3 or /dev/ttyUSB0): ")

    try:
        ser = serial.Serial(port_name, 9600, timeout=1)
        print(f"Successfully opened serial port {port_name} at 9600 baud.")
        print("Listening for data from Arduino Nano...")
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {port_name}.")
        print(e)
        sys.exit(1)

    # Start a thread to handle sending commands
    sender_handler = threading.Thread(target=command_sender_thread, args=(ser,), daemon=True)
    sender_handler.start()

    try:
        while not stop_threads:
            if ser.in_waiting > 0:
                line = ser.readline().decode('ascii', errors='ignore').strip()
                if line:
                    print(f"\n[RAW PACKET]: {line}")
                    parsed_data = parse_packet(line)
                    if parsed_data:
                        print(f"--- Parsed Packet: {parsed_data['type']} ---")
                        if parsed_data['type'] == "Sensor Data":
                            for key, value in parsed_data['data'].items():
                                print(f"  {key.replace('_', ' ').title()}: {value}")
                        else:
                            for key, value in parsed_data.items():
                                if key != 'type':
                                    print(f"  {key.replace('_', ' ').title()}: {value}")
                        print("--------------------------")
            time.sleep(0.05) # Small delay to prevent busy-waiting
    except KeyboardInterrupt:
        print("\nExiting Nano receiver simulator.")
    finally:
        stop_threads = True
        if ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()
