#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Servo.h>
#include <EEPROM.h>

// ================= Password Lock =================
#define PWnmbrLength 5  // รหัส 4 หลัก + '\0'
char Data[PWnmbrLength];
char Master[PWnmbrLength] = "1150";  // รหัสเริ่มต้น (จะถูกเขียนเก็บใน EEPROM ครั้งแรก)
byte data_count = 0;

// ====== ตั้งค่า "วันเดือนปีเกิด" ที่ต้องใช้ยืนยันก่อนรีเซ็ต ======
const char DOB_EXPECTED[] = "17102004";  // <<--- แก้ให้เป็นของคุณ รูปแบบ DDMMYYYY (8 หลัก)

// ====== EEPROM layout ======
const int EE_PASS_ADDR = 0;     // เก็บรหัส (รวม '\0' = 5 ไบต์)
const int EE_MAGIC_ADDR = 100;  // เก็บ magic byte เพื่อบอกว่าเคยตั้งค่าแล้ว
const byte EE_MAGIC_VAL = 0x42;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD address=0x27

// ----- Servo / Buzzer -----
int servoLockPin = 12;
Servo servoLock;
int buzzerLock = 10;

// ----- Keypad (PCF8574) -----
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 0, 1, 2, 3 };  // R1:P0 R2:P1 R3:P2 R4:P3
byte colPins[COLS] = { 4, 5, 6, 7 };  // C1:P4 C2:P5 C3:P6 C4:P7
#define KEYPAD_ADDR 0x20
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

// ====== โหมดการทำงาน ======
enum Mode { MODE_NORMAL,
            MODE_RESET_CHECK_DOB,
            MODE_RESET_NEW_PW };
Mode mode = MODE_NORMAL;

// ====== บัฟเฟอร์สำหรับรีเซ็ต ======
char dobBuf[9];  // 8 หลัก + '\0'
byte dobCount = 0;

char newPwBuf[PWnmbrLength];
byte newPwCount = 0;

// ---------- Helper: Buzzer ----------
void beepOK(unsigned ms = 150) {
  digitalWrite(buzzerLock, HIGH);
  delay(ms);
  digitalWrite(buzzerLock, LOW);
}
void beepError(unsigned ms = 150) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(buzzerLock, HIGH);
    delay(ms);
    digitalWrite(buzzerLock, LOW);
    delay(ms);
  }
}

// ---------- Helper: EEPROM ----------
void savePasswordToEEPROM(const char *pw) {
  // เก็บรหัส 4 หลัก + '\0'
  for (int i = 0; i < PWnmbrLength; i++) {
    EEPROM.update(EE_PASS_ADDR + i, pw[i]);
  }
  EEPROM.update(EE_MAGIC_ADDR, EE_MAGIC_VAL);
}

void loadPasswordFromEEPROM(char *out) {
  for (int i = 0; i < PWnmbrLength; i++) {
    out[i] = EEPROM.read(EE_PASS_ADDR + i);
  }
  // เผื่อกรณีข้อมูลไม่ครบ ใส่ null ท้าย
  out[PWnmbrLength - 1] = '\0';
}

// ---------- Helper: Clear buffers ----------
void clearData() {
  for (byte i = 0; i < PWnmbrLength; i++) Data[i] = '\0';
  data_count = 0;
}
void clearDOB() {
  for (byte i = 0; i < 9; i++) dobBuf[i] = '\0';
  dobCount = 0;
}
void clearNewPW() {
  for (byte i = 0; i < PWnmbrLength; i++) newPwBuf[i] = '\0';
  newPwCount = 0;
}

void showEnterPassword() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  clearData();
}

// ======================= Setup =======================
void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();

  keypad.begin();

  // Servo / Buzzer
  servoLock.attach(servoLockPin);
  servoLock.write(0);  // lock position
  pinMode(buzzerLock, OUTPUT);
  digitalWrite(buzzerLock, LOW);

  // Serial
  Serial.begin(9600);

  // ----- ตรวจ EEPROM ครั้งแรก -----
  if (EEPROM.read(EE_MAGIC_ADDR) != EE_MAGIC_VAL) {
    // ยังไม่เคยตั้งค่า -> เขียนค่าเริ่มต้นลง EEPROM
    savePasswordToEEPROM(Master);
  }

  // โหลดรหัสจาก EEPROM มาใส่ Master ทุกครั้งที่เปิดเครื่อง
  loadPasswordFromEEPROM(Master);

  showEnterPassword();
}

// ======================= Loop =======================
void loop() {
  char key = keypad.getKey();
  if (!key) {
    delay(50);
    return;
  }

  // --------- สวิตช์โหมดรีเซ็ตเมื่อกด '*' จากโหมดปกติ ---------
  if (mode == MODE_NORMAL && key == '*') {
    mode = MODE_RESET_CHECK_DOB;
    clearDOB();
    lcd.clear();
    lcd.print("DOB: DDMMYYYY");
    lcd.setCursor(0, 1);
    lcd.print("Enter: ");
    return;
  }

  switch (mode) {
    case MODE_NORMAL:
      {
        // --------- กรอกรหัสปกติ ---------
        if (key && key != '*') {
          // เก็บตัวอักษรที่กด
          if (data_count < PWnmbrLength - 1) {
            Data[data_count] = key;
            lcd.setCursor(data_count, 1);
            lcd.print('*');  // แสดง * แทนตัวเลข
            data_count++;
          }
        }

        // เมื่อครบ 4 หลัก -> ตรวจ
        if (data_count == PWnmbrLength - 1) {
          Data[data_count] = '\0';
          lcd.clear();
          if (!strcmp(Data, Master)) {
            lcd.print("Unlocking...");
            // Buzzer success
            digitalWrite(buzzerLock, HIGH);
            delay(500);
            digitalWrite(buzzerLock, LOW);

            // Servo เปิด 5 วิ แล้วล็อกกลับ
            servoLock.write(90);
            delay(5000);
            servoLock.write(0);

          } else {
            lcd.print("Incorrect!");
            Serial.println("WRONG_PASS");
            beepError(150);
          }
          clearData();
          delay(800);
          showEnterPassword();
        }
      }
      break;

    case MODE_RESET_CHECK_DOB:
      {
        // --------- รับ DOB 8 หลัก (0-9 เท่านั้น) ---------
        if (key >= '0' && key <= '9') {
          if (dobCount < 8) {
            dobBuf[dobCount++] = key;
            lcd.print('*');  // ซ่อนเป็น *
          }
          if (dobCount == 8) {
            dobBuf[8] = '\0';
            // ตรวจกับ DOB_EXPECTED
            if (strcmp(dobBuf, DOB_EXPECTED) == 0) {
              // ผ่าน -> ไปตั้งรหัสใหม่
              beepOK(120);
              mode = MODE_RESET_NEW_PW;
              clearNewPW();
              lcd.clear();
              lcd.print("New PIN (4):");
              lcd.setCursor(0, 1);
            } else {
              // ไม่ผ่าน
              lcd.clear();
              lcd.print("DOB wrong!");
              beepError(150);
              delay(1200);
              mode = MODE_NORMAL;
              showEnterPassword();
            }
          }
        } else if (key == '#') {
          // เผื่อผู้ใช้กดยกเลิก
          mode = MODE_NORMAL;
          showEnterPassword();
        }
      }
      break;

    case MODE_RESET_NEW_PW:
      {
        // --------- รับรหัสใหม่ 4 หลัก ---------
        if (key >= '0' && key <= '9') {
          if (newPwCount < PWnmbrLength - 1) {
            newPwBuf[newPwCount++] = key;
            lcd.print('*');  // ซ่อน
          }
          if (newPwCount == PWnmbrLength - 1) {
            newPwBuf[PWnmbrLength - 1] = '\0';

            // บันทึกลง EEPROM + อัปเดต Master ใน RAM
            savePasswordToEEPROM(newPwBuf);
            loadPasswordFromEEPROM(Master);

            lcd.clear();
            lcd.print("PIN Updated!");
            Serial.println("PASS_CHANGED");
            beepOK(250);
            delay(1200);

            // กลับโหมดปกติ
            mode = MODE_NORMAL;
            showEnterPassword();
          }
        } else if (key == '#') {
          // ยกเลิกการตั้งรหัสใหม่
          mode = MODE_NORMAL;
          showEnterPassword();
        }
      }
      break;
  }

  delay(50);
}
