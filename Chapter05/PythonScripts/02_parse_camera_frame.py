import numpy as np
import serial
import sys
import uuid
from PIL import Image

port = '/dev/ttyACM0'
baudrate = 115600

# Initialize serial port
ser = serial.Serial()
ser.port     = port
ser.baudrate = baudrate
ser.open()
ser.reset_input_buffer()

width  = 1
height = 1
num_ch = 3

image = np.empty((height, width, num_ch), dtype=np.uint8)

def serial_readline():
    data = ser.readline() # read a '\n' terminated line
    return data.decode("utf-8").strip()

while True:
    data_str = serial_readline()

    if str(data_str) == "<image>":
        w_str = serial_readline()
        h_str = serial_readline()
        w = int(w_str)
        h = int(h_str)
        if w != width or h != height:
            print("Resizing numpy array")
            if w * h != width * height:
                image.resize((h, w, num_ch))
            else:
                image.reshape((h, w, num_ch))
            width  = w
            height = h
        print("Reading frame:", width, height)
        for y in range(0, height):
            for x in range(0, width):
                for c in range(0, num_ch):
                    data_str = serial_readline()
                    image[y][x][c] = int(data_str)
        data_str = serial_readline()
        if str(data_str) == "</image>":
            print("Captured frame")
            image_pil = Image.fromarray(image)
            image_pil.show()
        else:
            print("Error capture image")
