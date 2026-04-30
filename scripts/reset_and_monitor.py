import serial
import time
import sys

def run():
    try:
        ser = serial.Serial('/dev/cu.usbmodem1101', 115200, timeout=1)
        ser.dtr = False
        ser.rts = False
        time.sleep(0.1)
        ser.dtr = True
        ser.rts = True
        time.sleep(0.1)
        ser.dtr = False
        ser.rts = False
        print("Reset sent. Listening...")
        end_time = time.time() + 10
        while time.time() < end_time:
            line = ser.readline()
            if line:
                sys.stdout.write(line.decode('utf-8', errors='replace'))
        ser.close()
    except Exception as e:
        print(f"Error: {e}")

run()
