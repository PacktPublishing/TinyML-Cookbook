import serial
import uuid
import struct
import wave
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("--label", dest="label", help="Label", type=str, required=True)
parser.add_argument("--port", dest="port", help="Port", type=str, required=True)
args = parser.parse_args()

port  = args.port  # e.g. /dev/ttyACM0
label = args.label # e.g. red

baudrate = 115200
sample_rate          = 16000
audio_len            = 1
audio_ch             = 1
num_bytes_per_sample = 2

num_records = 0
is_recording = False
audio_buffer = []
ser = serial.Serial()
ser.port     = port
ser.baudrate = baudrate
ser.open()
ser.flushInput()

while True:
    ser_bytes = ser.readline() # read a '\n' terminated line
    try:
        sample = int(ser_bytes[0:len(ser_bytes)-1].decode("utf-8"))
        if is_recording == False:
            is_recording = True
        audio_buffer.append(sample)
    except ValueError:
        if is_recording == True:
            is_recording = False
            filename = label + "_"+ str(uuid.uuid4()) + ".wav"
            print("Captured audio: [Sample-rate|Num-samples]", sample_rate, len(audio_buffer))
            audio_obj = wave.open(filename, 'wb')
            audio_obj.setnchannels(audio_ch)
            audio_obj.setframerate(sample_rate)
            audio_obj.setsampwidth(num_bytes_per_sample)
            for sample in audio_buffer:
                sample_bytes = struct.pack('<h', sample)
                audio_obj.writeframesraw(sample_bytes)
            audio_obj.close()
            num_records = num_records + 1
            audio_buffer.clear()
