import os
import serial
import subprocess
from datetime import datetime

# -------------------------
# Configurations
SERIAL_PORT = "/dev/ttyACM0"       # Arduino USB serial port
BAUD_RATE = 9600                   # Must match Arduino setting
CAMERA_ID = 2                      # Camera2
MEDIA_PATH = "/var/lib/motioneye/Camera2"
CAMERA_URL = f"http://localhost:8765/picture/{CAMERA_ID}/current/"

# -------------------------
def ensure_folder(path: str):
    # Folder permission ตั้งแล้วในระบบ, ไม่ต้อง chmod
    os.makedirs(path, exist_ok=True)

def generate_filename() -> str:
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{MEDIA_PATH}/failed_{ts}.jpg"

def capture_snapshot(filename: str):
    # ดาวน์โหลด snapshot จาก MotionEye
    subprocess.run(["wget", "-q", "-O", filename, CAMERA_URL])
    # ให้ MotionEye อ่านได้
    os.chmod(filename, 0o664)
    print(f"[OK] Snapshot saved: {filename}")

# -------------------------
def main():
    ensure_folder(MEDIA_PATH)

    # เปิด Serial port
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException as e:
        print(f"[ERROR] Cannot open serial port {SERIAL_PORT}: {e}")
        return

    print("[INFO] Waiting for WRONG_PASS from Arduino...")

    while True:
        try:
            line = ser.readline().decode(errors="ignore").strip()
        except Exception as e:
            print(f"[WARN] Serial read error: {e}")
            continue

        if line == "WRONG_PASS":
            print("[DEBUG] Received: WRONG_PASS")
            filename = generate_filename()
            try:
                capture_snapshot(filename)
            except Exception as e:
                print(f"[WARN] Failed to capture snapshot: {e}")

# -------------------------
if __name__ == "__main__":
    main()
