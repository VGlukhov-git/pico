import serial
import threading
import time

COM_PORT = "COM11"
BAUDRATE = 115200

STEP = 0.01
DELAY = 0.001

ser = serial.Serial(COM_PORT, BAUDRATE, timeout=1)
time.sleep(2)

def reader():
    while True:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line:
                print("RX:", line)
        except Exception:
            break

def set_servo(servo, angle):
    ser.write(f"{servo};{angle:.2f}\n".encode())

threading.Thread(target=reader, daemon=True).start()

angle = 0
direction = 1

try:
    while True:
        angle += STEP * direction

        set_servo(1, angle)
        set_servo(4, angle)
        if angle > 90:
            direction = -1

        if angle < 0:
            direction = 1

        time.sleep(DELAY)

except KeyboardInterrupt:
    print("Stopping...")

finally:
    ser.close()