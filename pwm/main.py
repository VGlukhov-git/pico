import os, time
from machine import Pin, SPI
# import st7789
# === Buttons ===
btn1 = Pin(2, Pin.IN, Pin.PULL_UP)
btn3 = Pin(3, Pin.IN, Pin.PULL_UP)
btn2 = Pin(17, Pin.IN, Pin.PULL_UP)
btn4 = Pin(15, Pin.IN, Pin.PULL_UP)

# === SPI & Display Init ===
spi = SPI(1, baudrate=40000000, sck=Pin(10), mosi=Pin(11))
cs  = Pin(9, Pin.OUT)
dc  = Pin(8, Pin.OUT)
rst = Pin(12, Pin.OUT)

def write_cmd(cmd):
    dc.value(0); cs.value(0); spi.write(bytearray([cmd])); cs.value(1)

def write_data(data):
    dc.value(1); cs.value(0); spi.write(data); cs.value(1)

def reset():
    rst.value(0); time.sleep_ms(50); rst.value(1); time.sleep_ms(50)

def init_display():
    reset()
    write_cmd(0x01); time.sleep_ms(150)   # SWRESET
    write_cmd(0x11); time.sleep_ms(500)   # SLPOUT
    write_cmd(0x36); write_data(bytearray([0x60]))  # rotation
    write_cmd(0x3A); write_data(bytearray([0x55]))  # RGB565
    write_cmd(0x21); write_cmd(0x13)
    write_cmd(0x29); time.sleep_ms(100)

def set_window(x0,y0,x1,y1):
    write_cmd(0x2A); write_data(bytearray([x0>>8,x0&0xFF,x1>>8,x1&0xFF]))
    write_cmd(0x2B); write_data(bytearray([y0>>8,y0&0xFF,y1>>8,y1&0xFF]))
    write_cmd(0x2C)

def show_raw_stream(path, w=160*2, h=240, chunk=8):
    set_window(0,0,w-1,h-1)
    with open(path,"rb") as f:
        for _ in range(0, h, chunk):
            data = f.read(w*2*chunk)
            write_data(data)

# === MAIN ===
init_display()

# All animation folders
ANIM_DIR = "/animations"
anim_folders = sorted([f for f in os.listdir(ANIM_DIR) if not f.startswith('.')])

anim_index = 0
frames = []

def load_frames():
    global frames
    folder = ANIM_DIR + "/" + anim_folders[anim_index]
    frames = sorted([f for f in os.listdir(folder) if f.endswith(".raw")])
    print("Loaded animation:", folder, frames)

load_frames()

def button_pressed(pin):
    return pin.value() == 0   # active low

while True:
    # check for next animation button
    if button_pressed(btn2) or button_pressed(btn4):
        anim_index = (anim_index + 1) % len(anim_folders)
        load_frames()
        time.sleep(0.3)  # debounce
    
    # play current animation frames
    folder = ANIM_DIR + "/" + anim_folders[anim_index]
    for frame in frames:
        show_raw_stream(folder + "/" + frame)
