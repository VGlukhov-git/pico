import ujson
import machine
import time

pin = machine.Pin(13)# 7
pin1 = machine.Pin(10)
pwm = machine.PWM(pin)
pwm1 = machine.PWM(pin1)
pwm.freq(50)
pwm1.freq(50)

min_duty = int(65535 * 0.5 / 20)
print(int(65535 * 2.4 / 20))
max_duty = 7864; #int(65535 * 2.4 / 20)


def angle_to_duty(angle):
    print(f"Setting angle: {angle}")
    return int(min_duty + (angle / 180) * (max_duty - min_duty))

print(angle_to_duty)
buffer = b""
a1 = 5

while True:
    pwm.duty_u16(angle_to_duty(180))  # Set to 90 degrees for testing
    pwm1.duty_u16(angle_to_duty(0))  # Set to 90 degrees for testing
    time.sleep(2)  # Wait for 1 second
    pwm.duty_u16(angle_to_duty(90))  # Set to 90 degrees for testing
    pwm1.duty_u16(angle_to_duty(90))  # Set to 90 degrees for testing
    time.sleep(2)  # Wait for 1 second
    pwm.duty_u16(angle_to_duty(0))   # Set to 0
    pwm1.duty_u16(angle_to_duty(180))   # Set to 0
    time.sleep(2)  # Wait for 1 second

    # pwm1.duty_u16(angle_to_duty(90 + a1))  # Set to 90 degrees for testing
    # pwm.duty_u16(k)   # Set to 0
    # k += 10
    # print(k)

    # time.sleep(0.02)  # Wait for 1 second