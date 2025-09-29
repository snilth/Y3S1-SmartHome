#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Servo.h>
#include <EEPROM.h>

/*** ===== Configs ===== ***/
#define USE_BUTTON 0

// PIR: false = active-LOW, true = active-HIGH
const bool PIR_ACTIVE_HIGH = false;

// LED2 ค้างหลังเจอ motion (ms)
const unsigned long LED2_HOLD_MS = 5000;

/*** ===== PIR Filter ===== ***/
const unsigned long REQ_ACTIVE_MS = 120;
const unsigned long REQ_IDLE_MS = 200;
const unsigned long COOLDOWN_MS = 500;

/*** ===== Pins ===== ***/
#define LED2 3
#define BTN 7
#define PIR 6
#define WATER_PIN A0
#define SERVO_VALVE_PIN 9  // Servo วาล์วน้ำ
#define SERVO_LOCK_PIN 12  // Servo ล็อกประตู
#define BUZZER_LOCK 10

/*** ===== Button (toggle LED2) ===== ***/
#if USE_BUTTON
bool ledOn = false;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;
int lastReading = HIGH;
int stableState = HIGH;
#endif

/*** ===== PIR (auto LED2) ===== ***/
unsigned long lastMotionMs = 0;
bool pirLedOn = false;

enum PirFSM { P_IDLE,
              P_QUALIFY_ACTIVE,
              P_TRIGGERED,
              P_QUALIFY_IDLE };
PirFSM pstate = P_IDLE;
unsigned long pMark = 0, lastTrigger = 0;

/*** ===== Water Sensor → Servo (WET=90° / DRY=0°) ===== ***/
const int WET_TRIG = 400;
const int DRY_TRIG = 300;
const int DRY_POS = 0;
const int WET_POS = 90;

int waterEMA = 0;
const uint8_t EMA_ALPHA = 32;
bool isWet = false;

Servo valve;
Servo servoLock;

/*** ================= Password Lock ================= ***/
#define PWnmbrLength 5

char Data[PWnmbrLength];
char Master[PWnmbrLength] = "1150";
byte data_count = 0;

// วันเดือนปีเกิด (DDMMYYYY) ใช้รีเซ็ต
const char DOB_EXPECTED[] = "17102004";

// EEPROM layout
const int EE_PASS_ADDR = 0;
const int EE_MAGIC_ADDR = 100;
const byte EE_MAGIC_VAL = 0x42;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };

#define KEYPAD_ADDR 0x20
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

enum Mode {
  MODE_NORMAL,
  MODE_RESET_CHECK_DOB,
  MODE_RESET_NEW_PW
};
Mode mode = MODE_NORMAL;

// Buffers
char dobBuf[9];
byte dobCount = 0;

char newPwBuf[PWnmbrLength];
byte newPwCount = 0;

/*** ================= Helper Functions ================= ***/
// ---------- Buzzer ----------
void beepOK(unsigned ms = 150) {
  digitalWrite(BUZZER_LOCK, HIGH);
  delay(ms);
  digitalWrite(BUZZER_LOCK, LOW);
}

void beepError(unsigned ms = 150) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_LOCK, HIGH);
    delay(ms);
    digitalWrite(BUZZER_LOCK, LOW);
    delay(ms);
  }
}

// ---------- EEPROM ----------
void savePasswordToEEPROM(const char *pw) {
  for (int i = 0; i < PWnmbrLength; i++) {
    EEPROM.update(EE_PASS_ADDR + i, pw[i]);
  }
  EEPROM.update(EE_MAGIC_ADDR, EE_MAGIC_VAL);
}

void loadPasswordFromEEPROM(char *out) {
  for (int i = 0; i < PWnmbrLength; i++) {
    out[i] = EEPROM.read(EE_PASS_ADDR + i);
  }
  out[PWnmbrLength - 1] = '\0';
}

// ---------- Buffers ----------
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

/*** ================= Setup ================= ***/
void setup() {
  // PIR + LED
  pinMode(LED2, OUTPUT);
#if USE_BUTTON
  pinMode(BTN, INPUT_PULLUP);
#endif
  if (PIR_ACTIVE_HIGH) pinMode(PIR, INPUT);
  else pinMode(PIR, INPUT_PULLUP);

  digitalWrite(LED2, LOW);

  // Servo valve
  valve.attach(SERVO_VALVE_PIN);
  valve.write(DRY_POS);
  isWet = false;
  waterEMA = analogRead(WATER_PIN);

  // LCD + Keypad
  Wire.begin();
  lcd.init();
  lcd.backlight();
  keypad.begin();

  // Servo Lock
  servoLock.attach(SERVO_LOCK_PIN);
  servoLock.write(0);

  // Buzzer
  pinMode(BUZZER_LOCK, OUTPUT);
  digitalWrite(BUZZER_LOCK, LOW);

  // Serial
  Serial.begin(9600);

  // EEPROM check
  if (EEPROM.read(EE_MAGIC_ADDR) != EE_MAGIC_VAL) {
    savePasswordToEEPROM(Master);
  }
  loadPasswordFromEEPROM(Master);

  showEnterPassword();
}

/*** ================= Loop ================= ***/
void loop() {
  unsigned long now = millis();

  /*** -------- Password Lock -------- ***/
  char key = keypad.getKey();
  if (key) {
    if (mode == MODE_NORMAL && key == '*') {
      mode = MODE_RESET_CHECK_DOB;
      clearDOB();
      lcd.clear();
      lcd.print("DOB: DDMMYYYY");
      lcd.setCursor(0, 1);
      lcd.print("Enter: ");
    } else {
      switch (mode) {
        case MODE_NORMAL:
          if (key && key != '*') {
            if (data_count < PWnmbrLength - 1) {
              Data[data_count] = key;
              lcd.setCursor(data_count, 1);
              lcd.print('*');
              data_count++;
            }
          }
          if (data_count == PWnmbrLength - 1) {
            Data[data_count] = '\0';
            lcd.clear();
            if (!strcmp(Data, Master)) {
              lcd.print("Unlocking...");
              digitalWrite(BUZZER_LOCK, HIGH);
              delay(500);
              digitalWrite(BUZZER_LOCK, LOW);
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
          break;

        case MODE_RESET_CHECK_DOB:
          if (key >= '0' && key <= '9') {
            if (dobCount < 8) {
              dobBuf[dobCount++] = key;
              lcd.print('*');
            }
            if (dobCount == 8) {
              dobBuf[8] = '\0';
              if (strcmp(dobBuf, DOB_EXPECTED) == 0) {
                beepOK(120);
                mode = MODE_RESET_NEW_PW;
                clearNewPW();
                lcd.clear();
                lcd.print("New PIN (4):");
                lcd.setCursor(0, 1);
              } else {
                lcd.clear();
                lcd.print("DOB wrong!");
                beepError(150);
                delay(1200);
                mode = MODE_NORMAL;
                showEnterPassword();
              }
            }
          } else if (key == '#') {
            mode = MODE_NORMAL;
            showEnterPassword();
          }
          break;

        case MODE_RESET_NEW_PW:
          if (key >= '0' && key <= '9') {
            if (newPwCount < PWnmbrLength - 1) {
              newPwBuf[newPwCount++] = key;
              lcd.print('*');
            }
            if (newPwCount == PWnmbrLength - 1) {
              newPwBuf[PWnmbrLength - 1] = '\0';
              savePasswordToEEPROM(newPwBuf);
              loadPasswordFromEEPROM(Master);
              lcd.clear();
              lcd.print("PIN Updated!");
              Serial.println("PASS_CHANGED");
              beepOK(250);
              delay(1200);
              mode = MODE_NORMAL;
              showEnterPassword();
            }
          } else if (key == '#') {
            mode = MODE_NORMAL;
            showEnterPassword();
          }
          break;
      }
    }
  }

  /*** -------- PIR -------- ***/
  int pirRaw = digitalRead(PIR);
  bool motionNow = ((!PIR_ACTIVE_HIGH) && pirRaw == LOW) || (PIR_ACTIVE_HIGH && pirRaw == HIGH);

  switch (pstate) {
    case P_IDLE:
      if (now - lastTrigger >= COOLDOWN_MS) {
        if (motionNow) {
          pstate = P_QUALIFY_ACTIVE;
          pMark = now;
        }
      }
      break;

    case P_QUALIFY_ACTIVE:
      if (!motionNow) {
        pstate = P_IDLE;
      } else if (now - pMark >= REQ_ACTIVE_MS) {
        lastTrigger = now;
        lastMotionMs = now;
        pirLedOn = true;
        pstate = P_TRIGGERED;
      }
      break;

    case P_TRIGGERED:
      if (pirLedOn && (now - lastMotionMs >= LED2_HOLD_MS)) pirLedOn = false;
      if (now - lastTrigger >= COOLDOWN_MS) {
        pstate = P_QUALIFY_IDLE;
        pMark = now;
      }
      break;

    case P_QUALIFY_IDLE:
      if (motionNow) {
        lastTrigger = now;
        pstate = P_TRIGGERED;
      } else if (now - pMark >= REQ_IDLE_MS) {
        pstate = P_IDLE;
      }
      break;
  }

#if USE_BUTTON
  if (ledOn) digitalWrite(LED2, HIGH);
  else digitalWrite(LED2, pirLedOn ? HIGH : LOW);
#else
  digitalWrite(LED2, pirLedOn ? HIGH : LOW);
#endif

  /*** -------- Water Sensor -------- ***/
  int waterRaw = analogRead(WATER_PIN);
  waterEMA += ((int32_t)EMA_ALPHA * (waterRaw - waterEMA)) >> 8;

  // log ค่าออก Serial (เพิ่ม timestamp)
  Serial.print("t=");
  Serial.print(millis());
  Serial.print(" ms  Raw=");
  Serial.print(waterRaw);
  Serial.print("  EMA=");
  Serial.print(waterEMA);
  Serial.print("  State=");
  Serial.println(isWet ? "WET" : "DRY");

  // เก็บสถานะก่อนหน้า
  bool prevIsWet = isWet;

  // ตรวจสอบสถานะใหม่
  if (!isWet && waterEMA >= WET_TRIG) {
    isWet = true;
  } else if (isWet && waterEMA <= DRY_TRIG) {
    isWet = false;
  }

  // ถ้า state เปลี่ยนเท่านั้น สั่ง Servo
  if (isWet != prevIsWet) {
    if (isWet) valve.write(WET_POS);  // WET = 90°
    else valve.write(DRY_POS);        // DRY = 0°
  }
}
