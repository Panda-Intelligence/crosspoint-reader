import serial
import time
import sys

port = '/dev/cu.usbmodem1101'
try:
    print(f"Opening {port} for reset...")
    ser = serial.Serial(port, 115200, timeout=1)
    
    # Toggle DTR/RTS to reset the ESP32-S3
    ser.dtr = False
    ser.rts = False
    time.sleep(0.1)
    ser.dtr = True
    ser.rts = True
    time.sleep(0.1)
    ser.dtr = False
    ser.rts = False
    
    print("Reset toggled. Listening for logs...")
    
    end_time = time.time() + 20
    while time.time() < end_time:
        line = ser.readline()
        if line:
            sys.stdout.write(line.decode('utf-8', errors='replace'))
    ser.close()
except Exception as e:
    print(f"Error: {e}")
