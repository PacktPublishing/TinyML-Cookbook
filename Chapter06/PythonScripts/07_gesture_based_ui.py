import pyautogui
import serial

port = '/dev/ttyACM0'
baudrate = 115600

# Initialize serial port
ser = serial.Serial()
ser.port     = port
ser.baudrate = baudrate
ser.open()
ser.reset_input_buffer()

def serial_readline():
    data = ser.readline() # read a '\n' terminated line
    return data.decode("utf-8").strip()

while True:
    data_str = serial_readline()

    # Mute/Unmute
    if str(data_str) == "circle":
        pyautogui.press('m')

    # Play/Pause video
    if str(data_str) == "cross":
        pyautogui.press('k')

    # Change video
    if str(data_str) == "pan":
        pyautogui.hotkey('shift', 'n')

