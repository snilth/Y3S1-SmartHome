#include <Servo.h>

/*** ===== Configs ===== ***/
#define USE_BUTTON 0

// PIR: false = active-LOW (ที่คุณทดสอบได้), true = active-HIGH
const bool PIR_ACTIVE_HIGH = false;

// LED2 ค้างหลังเจอ motion (ms)
const unsigned long LED2_HOLD_MS = 5000;

/*** ตัวกรอง PIR ***/
const unsigned long REQ_ACTIVE_MS = 120;
const unsigned long REQ_IDLE_MS   = 200;
const unsigned long COOLDOWN_MS   = 500;

/*** ===== Pins ===== ***/
#define LED2       3
#define BTN        7
#define PIR        6
#define WATER_PIN  A0
#define SERVO_PIN  9

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

// FSM กรองสัญญาณ PIR
enum PirFSM { P_IDLE, P_QUALIFY_ACTIVE, P_TRIGGERED, P_QUALIFY_IDLE };
PirFSM pstate = P_IDLE;
unsigned long pMark = 0, lastTrigger = 0;

/*** ===== Water Sensor → Servo (WET=90° / DRY=0°) ===== ***/
const int WET_TRIG = 600;   // >= เปียก
const int DRY_TRIG = 500;   // <= แห้ง (ต่ำกว่า WET_TRIG เพื่อทำฮิสเทอรีซิส)
const int DRY_POS  = 0;
const int WET_POS  = 90;

// EMA ลดสั่น
int waterEMA = 0;
const uint8_t EMA_ALPHA = 32;

// เก็บสถานะเปียก/แห้งปัจจุบัน
bool isWet = false;

Servo valve;

void setup() {
  pinMode(LED2, OUTPUT);
#if USE_BUTTON
  pinMode(BTN, INPUT_PULLUP);
#endif
  if (PIR_ACTIVE_HIGH) pinMode(PIR, INPUT);
  else                 pinMode(PIR, INPUT_PULLUP);

  digitalWrite(LED2, LOW);

  valve.attach(SERVO_PIN);
  valve.write(DRY_POS);  // เริ่มที่ 0°
  isWet = false;

  waterEMA = analogRead(WATER_PIN);
}

void loop() {
  unsigned long now = millis();

  /*** ===== Button (optional) ===== ***/
#if USE_BUTTON
  int reading = digitalRead(BTN);
  if (reading != lastReading) lastDebounceTime = now;
  if ((now - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) ledOn = !ledOn;
    }
  }
  lastReading = reading;
#endif

  /*** ===== PIR: filtered edge detection ===== ***/
  int pirRaw = digitalRead(PIR);
  bool motionLow  = (!PIR_ACTIVE_HIGH) && (pirRaw == LOW);
  bool motionHigh = (PIR_ACTIVE_HIGH)  && (pirRaw == HIGH);
  bool motionNow  = motionLow || motionHigh;

  switch (pstate) {
    case P_IDLE:
      if (now - lastTrigger >= COOLDOWN_MS) {
        if (motionNow) { pstate = P_QUALIFY_ACTIVE; pMark = now; }
      }
      break;
    case P_QUALIFY_ACTIVE:
      if (!motionNow) { pstate = P_IDLE; }
      else if (now - pMark >= REQ_ACTIVE_MS) {
        lastTrigger = now;
        lastMotionMs = now;
        pirLedOn = true;
        pstate = P_TRIGGERED;
      }
      break;
    case P_TRIGGERED:
      if (pirLedOn && (now - lastMotionMs >= LED2_HOLD_MS)) pirLedOn = false;
      if (now - lastTrigger >= COOLDOWN_MS) { pstate = P_QUALIFY_IDLE; pMark = now; }
      break;
    case P_QUALIFY_IDLE:
      if (motionNow) { lastTrigger = now; pstate = P_TRIGGERED; }
      else if (now - pMark >= REQ_IDLE_MS) { pstate = P_IDLE; }
      break;
  }

  /*** ===== คุม LED2 ===== ***/
#if USE_BUTTON
  if (ledOn) digitalWrite(LED2, HIGH);
  else       digitalWrite(LED2, pirLedOn ? HIGH : LOW);
#else
  digitalWrite(LED2, pirLedOn ? HIGH : LOW);
#endif

  /*** ===== Water Sensor → Servo (ฮิสเทอรีซิส: WET stays 90°, DRY stays 0°) ===== ***/
  int waterRaw = analogRead(WATER_PIN);
  waterEMA += ((int32_t)EMA_ALPHA * (waterRaw - waterEMA)) >> 8;

  // เปลี่ยนสถานะเฉพาะเมื่อ "ข้าม" ธรช. ฮิสเทอรีซิส เพื่อลดการกระพือของเซอร์โว
  if (!isWet && waterEMA >= WET_TRIG) {
    isWet = true;
    valve.write(WET_POS);    // ไป 90°
  } else if (isWet && waterEMA <= DRY_TRIG) {
    isWet = false;
    valve.write(DRY_POS);    // กลับ 0°
  }

  // (ออปชัน) delay(1);
}
