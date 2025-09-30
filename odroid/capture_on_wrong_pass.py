import serial          # For communicating with Arduino via serial port
import subprocess      # To run shell commands like wget
from datetime import datetime  # For working with dates and timestamps
import os              # For file and folder operations

# -------------------------
# Set up the serial port to communicate with Arduino
# '/dev/ttyACM0' → USB port where Arduino is connected
# 9600 → baud rate (must match Arduino's setting)
# timeout=1 → waits 1 second for data; prevents blocking indefinitely
ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)

# -------------------------
# Set the folder to save snapshots
# MotionEye will save snapshots from Camera2 to this folder
capture_folder = "/var/lib/motioneye/Camera2"
# Create the folder if it doesn't exist
os.makedirs(capture_folder, exist_ok=True)

# -------------------------
# Continuously read data from Arduino
while True:
    line = ser.readline().decode().strip()  # Read a line from Serial, decode to string, strip whitespace
    if line == "WRONG_PASS":                # Check if Arduino sent "WRONG_PASS"
        # ---------------------
        # Generate a filename with a timestamp to avoid overwriting
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{capture_folder}/failed_{ts}.jpg"

        # ---------------------
        # Call the MotionEye API to capture a snapshot from Camera2
        # Use wget to download the image and save it to the filename
        subprocess.run([
            "wget", "-q", "-O", filename,
            "http://localhost:8765/picture/2/current/"
        ])
        print(f"Captured snapshot {filename}")  # Print confirmation that the snapshot was saved
