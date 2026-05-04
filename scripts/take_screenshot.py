#!/usr/bin/env python3
import sys
import serial
import time
from PIL import Image

def take_screenshot(port="/dev/cu.usbmodem1101", output="screenshot.bmp"):
    try:
        # Open serial port
        ser = serial.Serial(port, 115200, timeout=2)
        
        print(f"Connected to {port}. Sending CMD:SCREENSHOT...")
        
        # Write exactly what the main.cpp is looking for (with \n)
        ser.write(b"CMD:SCREENSHOT\n")
        ser.flush()

        # Some debug info
        time.sleep(0.1)
        while ser.in_waiting > 0:
             line = ser.readline()
             # print(f"Debug: {line}")
             try:
                 text = line.decode('utf-8', errors='ignore').strip()
                 if "SCREENSHOT_START:" in text:
                     idx = text.find("SCREENSHOT_START:")
                     parts = text[idx:].split(":")
                     if len(parts) >= 2:
                         screenshot_size = int(parts[1])
                         print(f"Downloading {screenshot_size} bytes...")
                         
                         screenshot_data = b""
                         timeout_start = time.time()
                         while len(screenshot_data) < screenshot_size and time.time() - timeout_start < 5:
                             needed = screenshot_size - len(screenshot_data)
                             data = ser.read(needed)
                             if data:
                                 screenshot_data += data
                                 
                         print(f"Got {len(screenshot_data)} bytes. Parsing...")
                         try:
                             # SSD1677 panel native is 800x480 (landscape).
                             # Mofei firmware drives the panel in 480x800
                             # logical-portrait by applying rotation in
                             # GfxRenderer::rotateCoordinates(). The raw
                             # framebuffer dump is in the panel's physical
                             # orientation, so rotate 90 deg clockwise to
                             # match what the user actually sees on screen.
                             raw = Image.frombytes("1", (800, 480), screenshot_data)
                             img = raw.transpose(Image.ROTATE_270)
                             img.save(output)
                             print(f"Success! Screenshot saved to {output} (480x800 portrait)")
                         except Exception as e:
                             print(f"Failed to process image with PIL: {e}")
                             with open("screenshot.raw", "wb") as f:
                                 f.write(screenshot_data)
                         return
             except Exception as e:
                 print(f"Err {e}")
                 
        print("Timeout or no matching line.")

    except serial.SerialException as e:
        print(f"Serial Error: {e}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1101"
    output = sys.argv[2] if len(sys.argv) > 2 else "screenshot.bmp"
    take_screenshot(port, output)
