/***** SmartHome: Ultrasonic + Blynk + Timer + ISR + Watchdog (ESP8266) *****/
#include <Arduino.h>
#include <Ticker.h>

#define BLYNK_TEMPLATE_ID   "TMPL6CQA_FLZL"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN    "pCRjqSzxGIpAvDxbAtfKaiGGEpf8MU5T"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// ---------- Wi-Fi (ต้องเป็น 2.4 GHz) ----------
char ssid[] = "iPhone k";
char pass[] = "Khim.2005";

// ---------- พินตามการต่อ ----------
const int TRIG_PIN = 14;   // D5 → TRIG
const int ECHO_PIN = 12;   // D6 → ECHO (ผ่านตัวแบ่งแรงดัน 5V->3.3V)

// ---------- ตั้งค่า ----------
float THRESHOLD_CM = 10.0;        // ระยะตัดสินว่า “เข้าใกล้”
const float HYSTERESIS_CM = 5.0;  // ต้องถอย > THRESHOLD+5 ถึง re-arm
const uint8_t  REQ_CONSEC_NEAR = 3;      // ต้องพบใกล้ติดต่อกันกี่ครั้ง
const uint32_t COOLDOWN_MS      = 30000; // คูลดาวน์ 30 วินาที
const uint32_t MEAS_PERIOD_MS   = 100;   // ยิง TRIG ทุก 100ms
const uint32_t ECHO_TIMEOUT_US  = 25000; // timeout ~4m
const uint32_t PRINT_EVERY_MS   = 1000;  // พิมพ์ Serial ทุก 1s

// ---------- ตัวแปร Ultrasonic / Timer ----------
Ticker trigTicker;
volatile bool     echoHigh     = false;
volatile uint32_t tRise        = 0;
volatile uint32_t echoWidth    = 0;   // us
volatile bool     hasNewEcho   = false;

// ---------- สถานะเตือน ----------
bool alerted = false;
bool ARMED   = true;      // V4 ควบคุม ON/OFF แจ้งเตือน
uint8_t     consecNear   = 0;
uint32_t    lastAlertMs  = 0;
uint32_t    lastPrintMs  = 0;

// ---------- Blynk Datastreams ----------
/*
V2: Double   -> distance_cm (From Device)
V3: Double   -> threshold_cm (From App & To Device)
V4: Integer  -> armed 0/1    (From App & To Device)
Event name: "ultra_alert" (enable Push)
*/

// รับค่า threshold จากแอป
BLYNK_WRITE(V3) {
  float v = param.asFloat();
  if (v > 0) {
    THRESHOLD_CM = v;
    Serial.printf("[Blynk] THRESHOLD_CM = %.1f cm\n", THRESHOLD_CM);
  }
}

// รับค่า armed 0/1 จากแอป
BLYNK_WRITE(V4) {
  ARMED = (param.asInt() == 1);
  Serial.printf("[Blynk] ARMED = %d\n", ARMED);
}

// หลังออนไลน์ให้ซิงก์ค่าจากแอป และส่งค่าปัจจุบันกลับไปแอป
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V3, V4);
  Blynk.virtualWrite(V3, THRESHOLD_CM);
  Blynk.virtualWrite(V4, ARMED ? 1 : 0);
}

// ---------- ฟังก์ชัน Ultrasonic ----------
ICACHE_RAM_ATTR void echoISR() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    echoHigh = true;
    tRise = micros();
  } else { // FALLING
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

  // กันค้างถ้า echo ค้าง HIGH นานเกิน
  uint32_t tStart = micros();
  while (echoHigh && (micros() - tStart) < ECHO_TIMEOUT_US) { yield(); }
  if (echoHigh) echoHigh = false;
}

inline float usToCm(uint32_t us) {
  // ระยะ (ซม.) = (เวลาไป-กลับ µs / 2) / 29.1
  return (us / 2.0f) / 29.1f;
}

// ---------- Setup ----------
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("\nUltrasonic + Blynk + Timer + ISR + Watchdog (ESP8266)");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);
  trigTicker.attach_ms(MEAS_PERIOD_MS, fireTrig);

  ESP.wdtEnable(4000); // watchdog 4s
}

// ---------- Loop ----------
void loop() {
  Blynk.run();

  ESP.wdtFeed();
  yield();

  if (hasNewEcho) {
    noInterrupts();
    uint32_t width = echoWidth;
    hasNewEcho = false;
    interrupts();

    float dist = NAN;
    if (width > 0 && width < ECHO_TIMEOUT_US) dist = usToCm(width);

    uint32_t nowMs = millis();

    // ส่งค่าระยะขึ้น Blynk + พิมพ์บน Serial
    if (!isnan(dist)) Blynk.virtualWrite(V2, dist);
    if (nowMs - lastPrintMs >= PRINT_EVERY_MS) {
      lastPrintMs = nowMs;
      if (isnan(dist)) Serial.println("Distance: --- (no echo)");
      else { Serial.printf("Distance: %.1f cm\n", dist); }
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
          // Push ไปแอป (ต้องมี Event ultra_alert ใน Template)
          Blynk.logEvent("ultra_alert", "พบวัตถุ/สัตว์เข้าใกล้");
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
