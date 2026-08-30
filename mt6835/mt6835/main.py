import machine
import time

READ_COMMAND_MASK = 0x4000
ANGLE_REGISTER = 0x3FFF
register_address = ANGLE_REGISTER

class MT6835:
    def __init__(self, spi_bus=0, sck=18, mosi=19, miso=16, cs=17):
        self.spi = machine.SPI(spi_bus, baudrate=10_000_000, polarity=0, phase=0, 
                               sck=machine.Pin(sck), mosi=machine.Pin(mosi), miso=machine.Pin(miso))
        self.cs = machine.Pin(cs, machine.Pin.OUT, value=1)

    def read_angle(self):
        tx_data = bytearray([0x30, 0x03, 0x00, 0x00, 0x00]) 
        rx_data = bytearray(5)
        self.cs.value(0)
        self.spi.write_readinto(tx_data, rx_data)
        self.cs.value(1)
        raw_val = (rx_data[2] << 13) | (rx_data[3] << 5) | (rx_data[4] >> 3)
        return (raw_val * 360) / 2097152

class AS5047:
    def __init__(self, spi_bus=1, sck=10, mosi=11, miso=12, cs=13):
        self.spi = machine.SPI(spi_bus, baudrate=10_000_000, polarity=0, phase=1,
                               sck=machine.Pin(sck), mosi=machine.Pin(mosi), miso=machine.Pin(miso))
        self.cs = machine.Pin(cs, machine.Pin.OUT, value=1)
        self.ANGLE_REG = 0x3FFF

    def _calculate_parity(self, val):
        # Count bits to ensure Even Parity
        ones = 0
        for i in range(16):
            if (val >> i) & 1:
                ones += 1
        return ones % 2

    def read_angle(self):
        command = (READ_COMMAND_MASK | register_address) & 0x7FFF
        parity = bin(command).count("1") % 2
        if parity == 1:
            command |= 0x8000

        cmd_h = (command >> 8) & 0xFF
        cmd_l = command & 0xFF
        tx_data = bytearray([cmd_h, cmd_l])
        rx_data = bytearray(2)

        # 4. SPI Transaction
        self.cs.value(0)
        self.spi.write_readinto(tx_data, rx_data)
        self.cs.value(1)

        # 5. Process Result (Mask out bit 15 parity and bit 14 error)
        raw_val = ((rx_data[0] << 8) | rx_data[1]) & 0x3FFF
        return (raw_val * 360) / 16384

# --- Initialization ---
mt_sensor = MT6835()
as_sensor = AS5047()

print("Dual Sensor Start...")

while True:
    try:
        mt_val = mt_sensor.read_angle()
        as_val = as_sensor.read_angle()
        
        print(f"MT6835: {mt_val:8.3f}° | AS5047: {as_val:8.2f}°")
        time.sleep(0.001)
        
    except KeyboardInterrupt:
        break