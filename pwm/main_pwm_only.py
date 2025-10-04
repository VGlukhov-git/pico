import machine

uart = machine.UART(0, baudrate=115000*2, tx=machine.Pin(16), rx=machine.Pin(17))
status_pin = machine.Pin(25, machine.Pin.OUT)

servo_pins = {}
for i in range(16):
    if i in (16, 17):
        continue
    pin = machine.Pin(i)
    pwm = machine.PWM(pin)
    pwm.freq(50)
    servo_pins[i] = pwm

min_duty = int(65535 * 0.5 / 20) # Values for mg92b
max_duty = int(65535 * 2.4 / 20) # Values for mg92b

def angle_to_duty(angle):
    return int(min_duty + (angle / 180) * (max_duty - min_duty))


buffer = b""

while True:
    status_pin.off()
    
    if uart.any():
        char = uart.read(1)
        if char:
            if char == b'\n':
                decoded_buffer = buffer.decode().strip()
                status_pin.on()
                try:
                    parts = decoded_buffer.split(";")
                    value = float(parts[1])
                    motor_id = int(parts[0])
                    if 0 <= motor_id <= 15 and 0 <= value <= 180:
                        duty = angle_to_duty(value)
                        servo_pins[motor_id].duty_u16(duty)
                except Exception as e:
                    buffer = b"" 
                    continue
                buffer = b"" 
            else:
                buffer += char
