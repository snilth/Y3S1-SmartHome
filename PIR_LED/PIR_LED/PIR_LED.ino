#include <Servo.h>

// ===== Pins =====
#define LED2       3
#define BTN        7    // ปุ่มต่อกับ GND (ใช้ INPUT_PULLUP)
#define PIR        6    // เซ็นเซอร์ตรวจจับการเคลื่อนไหว
#define WATER_PIN  A0   // ขา Analog ของ Water/Rain/Leak Sensor
#define SERVO_PIN  9    // ขา PWM ของ Servo (เช่น SG90)

// ===== Button (toggle LED2) =====
bool ledOn = false;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;
int lastReading = HIGH;     // ปล่อยปุ่ม = HIGH
int stableState = HIGH;

// ===== PIR (auto LED2) =====
const unsigned long LED2_HOLD_MS = 5000;
unsigned long lastMotionMs = 0;
bool pirLedOn = false;

// ===== Water Sensor → Servo (90° และกลับหลัง 2 วิ) =====
const int WET_TRIG = 600;   // ปรับตามคาลิเบรต (0..1023) — ค่ามากกว่า = เปียก
const int DRY_TRIG = 500;   // ต้องต่ำกว่า WET_TRIG เพื่อทำฮิสเทอรีซิส
const int DRY_POS  = 0;     // มุมปกติ
const int WET_POS  = 90;    // มุมเมื่อเจอน้ำ

// กรองสัญญาณเพื่อลดสั่น (EMA)
int waterEMA = 0;
const uint8_t EMA_ALPHA = 32; // 0..255 (32≈0.125)

// สเตตเครื่องสำหรับทริกเกอร์ครั้งละ 1 หน
enum ServoState { DRY_READY, WET_HOLD, WAIT_DRY };
ServoState servoState = DRY_READY;
unsigned long wetStartMs = 0;

Servo valve;
int  servoNow = DRY_POS;

void setup() {
  pinMode(LED2, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(PIR, INPUT);
  digitalWrite(LED2, LOW);

  valve.attach(SERVO_PIN);
  valve.write(DRY_POS);
  servoNow = DRY_POS;

  waterEMA = analogRead(WATER_PIN); // ตั้งต้น EMA
}

void loop() {
  unsigned long now = millis();

  // ===== ปุ่ม + debounce (toggle LED2) =====
  int reading = digitalRead(BTN);
  if (reading != lastReading) lastDebounceTime = now;
  if ((now - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {
        ledOn = !ledOn;  // toggle
      }
    }
  }
  lastReading = reading;

  // ===== PIR → คุม LED2 (ถ้าไม่ได้เปิดด้วยปุ่ม) =====
  int pirState = digitalRead(PIR);
  if (pirState == HIGH) { pirLedOn = true; lastMotionMs = now; }
  else if (pirLedOn && (now - lastMotionMs >= LED2_HOLD_MS)) { pirLedOn = false; }

  if (ledOn) digitalWrite(LED2, HIGH);
  else       digitalWrite(LED2, pirLedOn ? HIGH : LOW);

  // ===== Water Sensor (Analog) → Servo 90° แล้วกลับหลัง 2 วิ =====
  int waterRaw = analogRead(WATER_PIN);
  waterEMA += ((int32_t)EMA_ALPHA * (waterRaw - waterEMA)) >> 8;

  switch (servoState) {
    case DRY_READY:
      // รอเหตุการณ์ "ขึ้น" (จากแห้ง → เปียก)
      if (waterEMA >= WET_TRIG) {
        valve.write(WET_POS);       // หมุนไป 90°
        servoNow = WET_POS;
        wetStartMs = now;           // เริ่มจับเวลา 2 วินาที
        servoState = WET_HOLD;
      }
      break;

    case WET_HOLD:
      // ค้างไว้ 2 วินาทีแล้วหมุนกลับ
      if (now - wetStartMs >= 2000UL) {
        valve.write(DRY_POS);       // กลับตำแหน่งปกติ
        servoNow = DRY_POS;
        servoState = WAIT_DRY;      // รอให้ค่ากลับไป "แห้ง" ก่อนค่อยพร้อมทริกใหม่
      }
      break;

    case WAIT_DRY:
      // ป้องกันทริกซ้ำตอนที่ยังเปียกค้าง → ต้องต่ำกว่า DRY_TRIG ก่อน
      if (waterEMA <= DRY_TRIG) {
        servoState = DRY_READY;
      }
      break;
  }

  // (ออปชัน) delay(2);
}
