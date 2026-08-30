import serial
import time

# CHANGE THIS to your Pico port
PORT = "COM4"          # Windows example
# PORT = "/dev/ttyACM0"  # Linux/macOS example

BAUDRATE = 115200      # Ignored by USB, but required by pyserial

ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)

# Give Pico time to reboot after opening port
time.sleep(2)

print("Connected to Pico. Type something...")

try:
    while True:
        # Read line from Pico (if available)
        if ser.in_waiting:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line:
                print("Pico:", line)

except KeyboardInterrupt:
    print("\nClosing port")
    ser.close()
