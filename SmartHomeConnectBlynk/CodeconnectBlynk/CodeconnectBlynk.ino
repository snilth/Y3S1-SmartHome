/***** SmartHome: Ultrasonic + Servo + RGB(zeRGBa+Power) + MQ-2 + Smoke LEDs+Buzzer + Blynk (ESP8266) *****/
#define BLYNK_TEMPLATE_ID   "TMPL6CQA_FLZL"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN    "pCRjqSzxGIpAvDxbAtfKaiGGEpf8MU5T"

#include <Ticker.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

/*** Wi-Fi ***/
char ssid[] = "iPhone k";
char pass[] = "Khim.2005";

/*** Ultrasonic ***/
const int TRIG_PIN = 16;   // D0 GPIO16
const int ECHO_PIN = 12;   // D6 GPIO12 (ผ่านตัวแบ่ง 5V->3.3V)

/*** Servo ***/
const int SERVO_PIN = 13;  // D7 GPIO13
Servo foodServo;
const uint32_t SERVO_PULSE_MS   = 500;    // เปิดค้าง 1000 ms
const uint32_t SERVO_LOCKOUT_MS = 200;    // กันสั่งถี่ 200 ms
int SERVO_CLOSED_US = 700;                // ปรับตามกลไกจริง
int SERVO_OPEN_US   = 2000;

bool     servoBusy        = false;
uint32_t servoActionStart = 0;
uint32_t servoLastDoneMs  = 0;

/*** RGB ***/
const int LED_R = 5;      // D1 GPIO5
const int LED_G = 4;      // D2 GPIO4
const int LED_B = 14;     // D5 GPIO14
const bool COMMON_ANODE = false;  // true ถ้า LED เป็น CA
uint8_t curR=0,curG=0,curB=0;
bool rgbPower=true;

/*** Smoke (MQ-2 + LEDs + Buzzer) ***/
#define MQ2_AO           A0
#define LED_SMOKE_AMBER  0   // D3  (คุณต่อไฟส้ม)
#define LED_SMOKE_RED    2   // D4  (คุณต่อไฟแดง)
#define BUZZER_PIN       15  // D8 Active buzzer (HIGH=on)

/* เลือกโพลาริตี LED ตามการต่อจริง:
   - Active-LOW (LOW=ติด, HIGH=ดับ) ให้ตั้ง true (เหมาะกับ D4 ที่มักเป็น LED บนบอร์ด)
   - Active-HIGH (HIGH=ติด, LOW=ดับ) ให้ตั้ง false (ถ้าใช้ LED ภายนอกต่อแบบธรรมดา)
*/
const bool LED_ACTIVE_LOW = false;   // <<< ปรับตรงนี้ให้ตรงกับการต่อจริง
inline void ledWrite(uint8_t pin, bool on) {
  if (LED_ACTIVE_LOW)  digitalWrite(pin, on ? LOW  : HIGH);
  else                 digitalWrite(pin, on ? HIGH : LOW);
}
inline void buzzerOn()  { digitalWrite(BUZZER_PIN, HIGH); }
inline void buzzerOff() { digitalWrite(BUZZER_PIN, LOW);  }

/*** Params (Ultrasonic) ***/
float THRESHOLD_CM             = 10.0;
const float HYSTERESIS_CM      = 5.0;
const uint8_t  REQ_CONSEC_NEAR = 2;
const uint32_t COOLDOWN_MS     = 500;
const uint32_t MEAS_PERIOD_MS  = 50;
const uint32_t ECHO_TIMEOUT_US = 25000;
const uint32_t PRINT_EVERY_MS  = 1000;

/*** MQ-2 thresholds (A0: 0..1023) ***/
int MQ2_ORANGE_RAW = 100;   // ค่าเริ่มต้นจากสเก็ตช์ทดสอบ
int MQ2_RED_RAW    = 400;
int MQ2_HYST_RAW   = 40;
const uint32_t MQ2_SAMPLE_MS            = 500;
const uint32_t SMOKE_ALERT_COOLDOWN_MS  = 15000;
const uint32_t BEEP_PERIOD_MS           = 400;
const uint32_t MQ2_WARMUP_MS            = 30000;  // กันแจ้งเตือนผิดระหว่างวอร์มอัพ 30s

/*** States ***/
Ticker ultraTicker;
volatile bool ultraTick=false;

bool ARMED = true;
bool alerted = false;
uint8_t  consecNear = 0;
uint32_t lastAlertMs = 0, lastPrintMs = 0;

uint32_t lastSmokeSample=0,lastSmokeAlert=0,lastBeepToggle=0, bootMs=0;
uint8_t  lastSmokeLevel=0;  // 0=OK, 1=AMBER, 2=RED
bool     beepOn=false;

/*** Helpers (RGB) ***/
inline void pwmWrite(uint8_t pin,uint8_t val){
  if(COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
void applyRGB(){
  if(!rgbPower){
    pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,0);
  }else{
    pwmWrite(LED_R,curR); pwmWrite(LED_G,curG); pwmWrite(LED_B,curB);
  }
}

/*** Helpers (Servo) ***/
bool canStartServoNow(){
  return (!servoBusy) && (millis() - servoLastDoneMs >= SERVO_LOCKOUT_MS);
}
void startServoPulseOpen(){
  if(!canStartServoNow()) return;
  servoBusy = true;
  servoActionStart = millis();
  foodServo.writeMicroseconds(SERVO_OPEN_US);
  Serial.println("[Servo] OPEN (us)");
}
void processServoPulse(){
  if(servoBusy && (millis()-servoActionStart >= SERVO_PULSE_MS)){
    foodServo.writeMicroseconds(SERVO_CLOSED_US);
    servoBusy = false;
    servoLastDoneMs = millis();
    Serial.println("[Servo] CLOSE (us)");
  }
}

/*** Blynk ***/
// V3 threshold_cm
BLYNK_WRITE(V3){ float v=param.asFloat(); if(v>0){ THRESHOLD_CM=v; Serial.printf("[Blynk] THRESHOLD_CM=%.1f\n",v);} }
// V4 armed
BLYNK_WRITE(V4){ ARMED = (param.asInt()==1); Serial.printf("[Blynk] ARMED=%d\n", ARMED); }
// V5 feed button (Push)
BLYNK_WRITE(V5){ if(param.asInt()==1) startServoPulseOpen(); }
// V8/V9/V10 zeRGBa
BLYNK_WRITE(V8){ curR = constrain(param.asInt(),0,255); applyRGB(); }
BLYNK_WRITE(V9){ curG = constrain(param.asInt(),0,255); applyRGB(); }
BLYNK_WRITE(V10){curB = constrain(param.asInt(),0,255); applyRGB(); }
// V12 RGB Power
BLYNK_WRITE(V12){ rgbPower = (param.asInt()==1); applyRGB(); }

BLYNK_CONNECTED(){
  Blynk.syncVirtual(V3,V4,V8,V9,V10,V12);
  Blynk.virtualWrite(V3, THRESHOLD_CM);
  Blynk.virtualWrite(V4, ARMED?1:0);
  Blynk.virtualWrite(V12, rgbPower?1:0);
}

/*** Ultrasonic ***/
void ultraTickCb(){ ultraTick = true; }
float readUltrasonicOnceCM(){
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if(dur==0) return NAN;
  return (float)dur * 0.01715f;    // (dur/2)/29.1
}

/*** Setup ***/
void setup(){
  pinMode(TRIG_PIN,OUTPUT); pinMode(ECHO_PIN,INPUT);
  pinMode(LED_R,OUTPUT); pinMode(LED_G,OUTPUT); pinMode(LED_B,OUTPUT);
  pinMode(LED_SMOKE_AMBER,OUTPUT);
  pinMode(LED_SMOKE_RED,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  // ดับไฟควันทั้งหมดตามโพลาริตี + ปิดบัซเซอร์
  ledWrite(LED_SMOKE_AMBER, false);
  ledWrite(LED_SMOKE_RED,   false);
  buzzerOff();

  analogWriteRange(255);

  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP8266 SmartHome (Ultrasonic + Servo + RGB + MQ-2)");

  // WiFi + Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Ultrasonic ticker (set flag only)
  ultraTicker.attach_ms(MEAS_PERIOD_MS, ultraTickCb);

  // Servo attach แบบ µs + ย้ำตำแหน่งปิด
  foodServo.attach(SERVO_PIN, 500, 2500);
  foodServo.writeMicroseconds(SERVO_CLOSED_US);
  delay(200);
  foodServo.writeMicroseconds(SERVO_CLOSED_US);

  applyRGB();

  bootMs = millis();
  Serial.println("[MQ-2] Warm-up ~30s (no alert during warm-up)...");
}

/*** Loop ***/
void loop(){
  Blynk.run();
  processServoPulse();

  // -------- Ultrasonic --------
  if(ultraTick){
    ultraTick = false;
    float d = readUltrasonicOnceCM();
    uint32_t now = millis();
    if(!isnan(d)) Blynk.virtualWrite(V2, d);
    if(now - lastPrintMs >= PRINT_EVERY_MS){
      lastPrintMs = now;
      if(isnan(d)) Serial.println("Distance: --- (no echo)");
      else         Serial.printf("Distance: %.1f cm\n", d);
    }
    if(!isnan(d)){
      bool isNear = (d < THRESHOLD_CM);
      if(isNear){
        if(consecNear < 255) consecNear++;
        if(ARMED && !alerted && consecNear >= REQ_CONSEC_NEAR &&
           (now - lastAlertMs > COOLDOWN_MS)){
          alerted = true; lastAlertMs = now;
          Blynk.logEvent("ultra_alert", "พบสัตว์/วัตถุเข้าใกล้");
          Serial.println("[Ultra] ALERT");
        }
      }else{
        consecNear = 0;
        if(alerted && d > (THRESHOLD_CM + HYSTERESIS_CM)){
          alerted = false;
          Serial.println("[Ultra] CLEARED");
        }
      }
    }
  }

  // -------- MQ-2 (sample + hysteresis + alert cooldown + beep) --------
  uint32_t now = millis();

  // วอร์มอัพ: ไม่แจ้งเตือน/ไม่เปิดไฟ/ไม่ร้อง ระหว่าง 30s แรก (แต่ยังส่ง V6 ได้)
  bool mq2Warmup = (now - bootMs < MQ2_WARMUP_MS);

  if(now - lastSmokeSample >= MQ2_SAMPLE_MS){
    lastSmokeSample = now;
    int raw = analogRead(MQ2_AO);      // 0..1023
    Blynk.virtualWrite(V6, raw);

    uint8_t lvl = lastSmokeLevel;

    if(!mq2Warmup){ // เฉพาะหลังวอร์มอัพ
      if      (lvl == 0) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw >= MQ2_ORANGE_RAW) lvl = 1; }
      else if (lvl == 1) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw <= MQ2_ORANGE_RAW - MQ2_HYST_RAW) lvl = 0; }
      else /*2*/        { if(raw <= MQ2_RED_RAW - MQ2_HYST_RAW) lvl = 1; }
    }else{
      lvl = 0; // ระหว่างอุ่นเซ็นเซอร์ ถือว่า OK เสมอ
    }

    if(lvl != lastSmokeLevel){
      lastSmokeLevel = lvl;
      Blynk.virtualWrite(V7, lvl);

      // LED ตามระดับ
      ledWrite(LED_SMOKE_AMBER, (lvl >= 1));
      ledWrite(LED_SMOKE_RED,   (lvl >= 2));

      // จัดการบัซเซอร์
      if(lvl < 2){ buzzerOff(); beepOn = false; }

      if(lvl == 2){
        Serial.printf("[Smoke] RED  raw=%d\n", raw);
        if(now - lastSmokeAlert >= SMOKE_ALERT_COOLDOWN_MS){
          lastSmokeAlert = now;
          Blynk.logEvent("smoke_alert", "ตรวจพบควันระดับสูง (RED)");
        }
      } else if(lvl == 1){
        Serial.printf("[Smoke] AMBER raw=%d\n", raw);
      } else {
        Serial.printf("[Smoke] OK    raw=%d\n", raw);
      }
    }
  }

  // Beep เป็นจังหวะเมื่อ RED
  if(!mq2Warmup && lastSmokeLevel == 2){
    if(now - lastBeepToggle >= BEEP_PERIOD_MS){
      lastBeepToggle = now;
      beepOn = !beepOn;
      if (beepOn) buzzerOn(); else buzzerOff();
    }
  }else{
    // ช่วงวอร์มอัพ/ไม่แดง -> ปิดเสียงไว้
    if (beepOn){ buzzerOff(); beepOn=false; }
  }
}
