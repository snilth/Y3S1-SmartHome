/***** SmartHome: Ultrasonic + Blynk + Servo + Timer + ISR + Watchdog (ESP8266) *****/
#include <Arduino.h>
#include <Ticker.h>
#include <Servo.h>

#define BLYNK_TEMPLATE_ID   "TMPL6CQA_FLZL"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN    "pCRjqSzxGIpAvDxbAtfKaiGGEpf8MU5T"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// ---------- Wi-Fi ----------
char ssid[] = "iPhone k";
char pass[] = "Khim.2005";

// ---------- Ultrasonic pins ----------
const int TRIG_PIN = 14;   // D5 → TRIG
const int ECHO_PIN = 12;   // D6 → ECHO (ผ่านตัวแบ่ง 5V->3.3V)

// ---------- Servo pin ----------
const int SERVO_PIN = 13;  // D7 → Servo Signal

// ---------- Parameters ----------
float THRESHOLD_CM = 10.0;
const float HYSTERESIS_CM   = 5.0;
const uint8_t  REQ_CONSEC_NEAR = 2;       // ต้องเจอใกล้ติดกัน 2 ครั้ง
const uint32_t COOLDOWN_MS      = 900;   // คูลดาวน์ 0.9 วิ
const uint32_t MEAS_PERIOD_MS   = 50;     // ยิง TRIG ทุก 50 ms
const uint32_t ECHO_TIMEOUT_US  = 25000;
const uint32_t PRINT_EVERY_MS   = 1000;

// ---------- Ultrasonic state ----------
Ticker trigTicker;
volatile bool     echoHigh   = false;
volatile uint32_t tRise      = 0;
volatile uint32_t echoWidth  = 0;
volatile bool     hasNewEcho = false;

// ---------- Alert state ----------
bool alerted = false;
bool ARMED   = true;        // V4
uint8_t  consecNear  = 0;
uint32_t lastAlertMs = 0;
uint32_t lastPrintMs = 0;

// ---------- Servo control (non-blocking pulse open/close) ----------
Servo foodServo;
const int SERVO_CLOSED = 10;       // มุมปิด (ตามกลไกจริงปรับได้)
const int SERVO_OPEN   = 170;     // มุมเปิด
const uint32_t SERVO_PULSE_MS  = 150;  // เปิดค้าง 150 ms (แทน delay(150))
const uint32_t SERVO_LOCKOUT_MS = 200; // กันสั่งถี่ 200 ms (แทน delay(200))

bool     servoBusy        = false;
uint32_t servoActionStart = 0;
uint32_t servoLastDoneMs  = 0;

// คิว 1 งาน: ถ้ากดระหว่าง busy/lockout จะเก็บคิวไว้ให้ทำต่อ
bool queuedFeed = false;

void startServoPulseOpen() {
  uint32_t now = millis();
  if (servoBusy) return;
  if (now - servoLastDoneMs < SERVO_LOCKOUT_MS) return; // กันสั่งซ้ำถี่
  servoBusy = true;
  servoActionStart = now;
  foodServo.write(SERVO_OPEN);
  Serial.println("[Servo] OPEN");
}

void processServoPulse() {
  if (servoBusy && (millis() - servoActionStart >= SERVO_PULSE_MS)) {
    foodServo.write(SERVO_CLOSED);
    servoBusy = false;
    servoLastDoneMs = millis(); // เริ่มนับ lockout 200ms
    Serial.println("[Servo] CLOSE");
  }
}

// ---------- Blynk Datastreams ----------
/*
V2 Double  -> distance_cm (From Device)
V3 Double  -> threshold_cm (From App & To Device)
V4 Integer -> armed 0/1    (From App & To Device)
V5 Integer -> feed_button  (From App; 1=feed once)
Event: "ultra_alert"
*/

BLYNK_WRITE(V3) { // ตั้ง threshold จากแอป
  float v = param.asFloat();
  if (v > 0) { THRESHOLD_CM = v; Serial.printf("[Blynk] THRESHOLD_CM = %.1f cm\n", THRESHOLD_CM); }
}

BLYNK_WRITE(V4) { // สวิตช์ armed
  ARMED = (param.asInt() == 1);
  Serial.printf("[Blynk] ARMED = %d\n", ARMED);
}

BLYNK_WRITE(V5) { // ปุ่มให้อาหารแบบ Push
  if (param.asInt() == 1) startServoPulseOpen();
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V3, V4);
  Blynk.virtualWrite(V3, THRESHOLD_CM);
  Blynk.virtualWrite(V4, ARMED ? 1 : 0);
}

// ---------- Ultrasonic helpers ----------
ICACHE_RAM_ATTR void echoISR() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    echoHigh = true;
    tRise = micros();
  } else {
    if (echoHigh) {
      uint32_t tFall = micros();
      echoWidth  = (tFall - tRise);
      hasNewEcho = true;
      echoHigh   = false;
    }
  }
}

void fireTrig() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  uint32_t tStart = micros();
  while (echoHigh && (micros() - tStart) < ECHO_TIMEOUT_US) { yield(); }
  if (echoHigh) echoHigh = false;
}

inline float usToCm(uint32_t us) { return (us / 2.0f) / 29.1f; }

// ---------- Setup ----------
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("\nUltrasonic + Blynk + Servo + Timer/ISR + WDT (ESP8266)");

  // Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Ultrasonic
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);
  trigTicker.attach_ms(MEAS_PERIOD_MS, fireTrig);

  // Servo
  foodServo.attach(SERVO_PIN);      // D7 (GPIO13)
  foodServo.write(SERVO_CLOSED);    // เริ่มต้นปิด

  // Watchdog
  ESP.wdtEnable(4000);
}

// ---------- Loop ----------
void loop() {
  Blynk.run();
  ESP.wdtFeed();
  yield();

  // ทำงานเซอร์โวแบบไม่บล็อก
  processServoPulse();

  if (hasNewEcho) {
    noInterrupts();
    uint32_t width = echoWidth;
    hasNewEcho = false;
    interrupts();

    float dist = NAN;
    if (width > 0 && width < ECHO_TIMEOUT_US) dist = usToCm(width);

    uint32_t nowMs = millis();

    // ส่งค่าไปแอป + พิมพ์ดีบัก
    if (!isnan(dist)) Blynk.virtualWrite(V2, dist);
    if (nowMs - lastPrintMs >= PRINT_EVERY_MS) {
      lastPrintMs = nowMs;
      if (isnan(dist)) Serial.println("Distance: --- (no echo)");
      else             Serial.printf("Distance: %.1f cm\n", dist);
    }

    // ตัดสินใจแจ้งเตือน (edge-trigger + consecutive + cooldown + hysteresis + armed)
    if (!isnan(dist)) {
      bool isNear = (dist < THRESHOLD_CM);

      if (isNear) {
        if (consecNear < 255) consecNear++;
        if (ARMED && !alerted && consecNear >= REQ_CONSEC_NEAR &&
            (nowMs - lastAlertMs > COOLDOWN_MS)) {

          alerted     = true;
          lastAlertMs = nowMs;

          Serial.printf("ALERT: พบวัตถุ/สัตว์เข้าใกล้ < %.1f cm\n", THRESHOLD_CM);
          Blynk.logEvent("ultra_alert", "พบวัตถุ/สัตว์เข้าใกล้");
          // ถ้าต้องการให้อาหารอัตโนมัติเมื่อพบแมว ให้เปิดบรรทัดนี้:
          // startServoPulseOpen();
        }
      } else {
        consecNear = 0;
        if (alerted && dist > (THRESHOLD_CM + HYSTERESIS_CM)) {
          alerted = false;
          Serial.println("CLEARED: ระยะปลอดภัยแล้ว (re-armed)");
        }
      }
    }
  }
}
