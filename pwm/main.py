import os
import sdcard

import uasyncio as asyncio
from machine import Pin, SPI

def remove_folder(path):
    """
    Recursively delete a folder and all its contents.
    Works like `rm -rf` in Linux.
    """
    try:
        for entry in os.ilistdir(path):
            name = entry[0]
            full_path = path + "/" + name
            if entry[1] == 0x4000:   # directory flag
                remove_folder(full_path)   # recurse
            else:
                os.remove(full_path)       # delete file
        os.rmdir(path)  # remove now-empty folder
        print("Removed folder:", path)
    except Exception as e:
        print("Error removing", path, ":", e)

# remove_folder("animations")
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

# === SD Card
spi0 = SPI(0, baudrate=200000000, sck=Pin(18), mosi=Pin(19), miso=Pin(16))
cs0 = Pin(17, Pin.OUT)
sd = sdcard.SDCard(spi0, cs0)

vfs = os.VfsFat(sd)
os.mount(vfs, "/sd")

print( os.listdir("/sd/animations_raw"))
print("Mounted SD card.")
#

def write_cmd(cmd):
    dc.value(0); cs.value(0); spi.write(bytearray([cmd])); cs.value(1)

def write_data(data):
    dc.value(1); cs.value(0); spi.write(data); cs.value(1)

def reset():
    rst.value(0); asyncio.sleep_ms(50); rst.value(1); asyncio.sleep_ms(50)

def init_display():
    reset()
    write_cmd(0x01); asyncio.sleep_ms(10)
    write_cmd(0x11); asyncio.sleep_ms(100)
    write_cmd(0x36); write_data(bytearray([0x60]))
    write_cmd(0x3A); write_data(bytearray([0x55]))
    write_cmd(0x21); write_cmd(0x13)
    write_cmd(0x29); asyncio.sleep_ms(100)

def set_window(x0,y0,x1,y1):
    write_cmd(0x2A); write_data(bytearray([x0>>8,x0&0xFF,x1>>8,x1&0xFF]))
    write_cmd(0x2B); write_data(bytearray([y0>>8,y0&0xFF,y1>>8,y1&0xFF]))
    write_cmd(0x2C)

async def show_raw_stream(path, w=320, h=240, chunk=242):
    set_window(0,0,w-1,h-1)
    with open(path,"rb") as f:
        for _ in range(0, h, chunk):
            data = f.read(w*2*chunk)
            write_data(data)
            await asyncio.sleep(0)

init_display()

ANIM_DIR = "/sd/animations_raw"
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
    return pin.value() == 0

async def play_animation():
    global anim_index  
    while True:
        folder = ANIM_DIR + "/" + anim_folders[anim_index]
        for frame in frames:
            await asyncio.sleep(0.05)
            await show_raw_stream(folder + "/" + frame)

async def read_sensor():
    global anim_index
    while True:
        if button_pressed(btn2) or button_pressed(btn4):
            anim_index = (anim_index + 1) % len(anim_folders)
            load_frames()
        await asyncio.sleep(0.01)

async def main():
    asyncio.create_task(play_animation())
    asyncio.create_task(read_sensor())
    while True:
        await asyncio.sleep(3600)

asyncio.run(main())
