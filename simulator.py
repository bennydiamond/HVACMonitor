import serial
import time
import threading
import sys

# --- Configuration ---
SEND_INTERVAL_SECONDS = 2

# --- Global variables ---
current_pressure = 50.0
current_cpm = 25
current_temperature = 21.5
current_humidity = 45.0
current_co2 = 450
current_voc_raw = 27500  # A typical raw VOC value
current_amps = 0.0 # <-- New variable for amps
current_pm1_0 = 5.2
current_pm2_5 = 8.9
current_pm4_0 = 10.1
current_pm10_0 = 12.5
stop_threads = False

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

def format_packet(p, cpm, t, h, co2, voc, amps, pm1, pm25, pm4, pm10):
    """Formats the data into the <pressure,cpm,temp,humi,co2,voc,amps,pm...,checksum>\\n protocol."""
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
    payload = f"{p_val},{cpm},{t_val},{h_val},{co2_val},{int(voc)},{amps_val},{pm1_val},{pm25_val},{pm4_val},{pm10_val}"
    data_part = f"S{payload}"
    checksum = calculate_checksum(data_part)
    return f"<{data_part},{checksum}>\n"

def user_input_thread():
    """A separate thread to handle user input without blocking."""
    global current_pressure, current_cpm, current_temperature, current_humidity
    global current_co2, current_voc_raw, current_amps, stop_threads
    global current_pm1_0, current_pm2_5, current_pm4_0, current_pm10_0
    
    print("\n--- Interactive Simulator Control ---")
    print("Commands:")
    print("  <number>          - Set pressure (e.g., 150.5)")
    print("  amps <number>     - Set AC Current in Amps (e.g., amps 1.5)")
    print("  cpm <number>      - Set CPM (e.g., cpm 350)")
    print("  temp <number>     - Set temperature (e.g., temp 23.8)")
    print("  humi <number>     - Set humidity (e.g., humi 55.2)")
    print("  co2 <number>      - Set CO2 in ppm (e.g., co2 800)")
    print("  voc <number>      - Set raw VOC value (e.g., voc 28000)")
    print("  pm1 <number>      - Set PM1.0 (e.g., pm1 5.2)")
    print("  pm25 <number>     - Set PM2.5 (e.g., pm25 9.8)")
    print("  pm4 <number>      - Set PM4.0 (e.g., pm4 11.3)")
    print("  pm10 <number>     - Set PM10.0 (e.g., pm10 15.1)")
    print("  quit              - Exit the simulator")
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
            elif cmd_type == 'cpm' and len(parts) > 1:
                current_cpm = int(parts[1])
                print(f"--> [SIM] CPM set to {current_cpm}")
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
    """A thread to continuously read and print data from the serial port."""
    global stop_threads
    while not stop_threads:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"[ESP32 DEBUG]: {line}")
        except serial.SerialException:
            print("--> [SIM-ERROR] Serial port disconnected.")
            stop_threads = True
            break
        except Exception as e:
            print(f"--> [SIM-ERROR] An error occurred in reader thread: {e}")
            stop_threads = True
            break
        time.sleep(0.05)


def main():
    """Main function to run the simulator."""
    global stop_threads
    
    port_name = ""
    if len(sys.argv) > 1:
        port_name = sys.argv[1]
    else:
        port_name = input("Enter the COM port of your USB-to-Serial adapter (e.g., COM3 or /dev/ttyUSB0): ")
    
    try:
        ser = serial.Serial(port_name, 9600, timeout=1)
        print(f"Successfully opened serial port {port_name} at 9600 baud.")
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
            # Pass the new current_amps variable to the packet formatter
            packet = format_packet(
                current_pressure, current_cpm, current_temperature, current_humidity,
                current_co2, current_voc_raw, current_amps,
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