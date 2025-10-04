import machine
import time

uart = machine.UART(0, baudrate=115000*2,
                    tx=machine.Pin(16), rx=machine.Pin(17))
status_pin = machine.Pin(25, machine.Pin.OUT)

potentiometer_left = machine.ADC(26)
potentiometer_right = machine.ADC(27)

servo_pins = {}
for i in range(16):
    if i in (16, 17):
        continue
    pin = machine.Pin(i)
    pwm = machine.PWM(pin)
    pwm.freq(50)
    servo_pins[i] = pwm

min_duty = int(65535 * 0.5 / 20)  # Values for mg92b
max_duty = int(65535 * 2.4 / 20)  # Values for mg92b


def angle_to_duty(angle):
    return int(min_duty + (angle / 180) * (max_duty - min_duty))


buffer = b""


def read_adc(adc):
    time.sleep_us(10)
    return adc.read_u16() * 3.3 / 65535  # in volts


def set_pwm(pwm, angle):
    duty = angle_to_duty(angle)
    pwm.duty_u16(duty)


angle = 100
set_pwm(servo_pins[0], angle)
set_pwm(servo_pins[1], angle)

limit = 2
while True:
    time.sleep_us(10)
    status_pin.on()
    value = read_adc(potentiometer_left)
    value1 = read_adc(potentiometer_right)
    print(value)
    print(angle)
    if (value > limit - 0.1):
        angle -= 0.01
    if (value < limit + 0.1):
        angle += 0.01
    if (angle > 180):
        angle = 180
    if (angle < 100):
        angle = 100
    set_pwm(servo_pins[0], 180 - angle)
    set_pwm(servo_pins[1], angle)

    # print(read_adc(potentiometer_left))

    # if uart.any():
    #     char = uart.read(1)
    #     if char:
    #         if char == b'\n':
    #             decoded_buffer = buffer.decode().strip()
    #             status_pin.on()
    #             try:
    #                 parts = decoded_buffer.split(";")
    #                 value = float(parts[1])
    #                 motor_id = int(parts[0])
    #                 # if 0 <= motor_id <= 15 and 0 <= value <= 180:
    #                     # duty = angle_to_duty(value)
    #                     # servo_pins[motor_id].duty_u16(duty)
    #             except Exception as e:
    #                 buffer = b""
    #                 continue
    #             buffer = b""
    #         else:
    #             buffer += char
