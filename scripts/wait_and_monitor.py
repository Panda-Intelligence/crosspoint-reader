import serial
import time
import sys
import glob

print("Waiting for /dev/cu.usbmodem* to appear...")
port = None
end_time = time.time() + 15
while time.time() < end_time:
    ports = glob.glob('/dev/cu.usbmodem*')
    if ports:
        port = ports[0]
        break
    time.sleep(0.5)

if not port:
    print("Device did not appear in time.")
    sys.exit(1)

print(f"Connecting to {port}...")
try:
    ser = serial.Serial(port, 115200, timeout=1)
    end_time = time.time() + 30
    while time.time() < end_time:
        line = ser.readline()
        if line:
            sys.stdout.write(line.decode('utf-8', errors='replace'))
    ser.close()
except Exception as e:
    print(f"Error: {e}")

