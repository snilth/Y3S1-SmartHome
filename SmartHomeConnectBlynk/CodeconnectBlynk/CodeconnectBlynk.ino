/***** SmartHome: Ultrasonic + Servo + RGB + MQ-2 + Blynk (ESP8266) *****/
#define BLYNK_TEMPLATE_ID   "TMPL6CQA_FLZL"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN    "pCRjqSzxGIpAvDxbAtfKaiGGEpf8MU5T"

#include <Ticker.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

/*** Wi-Fi ***/
char ssid[] = "iPhone k";
char pass[] = "Khim.2005";

/*** Ultrasonic ***/
const int TRIG_PIN = 16;   // D0 trigger
const int ECHO_PIN = 12;   // D6 (ผ่านตัวแบ่ง 5V->3.3V => r1k+r2k)

/*** Servo (Feeder) ***/
const int SERVO_PIN = 13;  // D7 สำหรับ Servo
Servo foodServo;
const uint32_t SERVO_PULSE_MS   = 500;  // เวลาที่เปิดฝา (ms)
const uint32_t SERVO_LOCKOUT_MS = 200;  // เวลาป้องกันการสั่งซ้ำเร็วเกินไป
int SERVO_CLOSED_US = 700;              // ค่า PWM microseconds ตอนปิด
int SERVO_OPEN_US   = 2000;             // ค่า PWM microseconds ตอนเปิด

bool     servoBusy        = false;  // flag กำลังทำงานอยู่
uint32_t servoActionStart = 0;      // เวลาเริ่มเปิด
uint32_t servoLastDoneMs  = 0;      // เวลาเสร็จรอบล่าสุด

/*** RGB ***/
const int LED_R = 5;      // D1
const int LED_G = 4;      // D2
const int LED_B = 14;     // D5
const bool COMMON_ANODE = false;  // กำหนดชนิด LED
uint8_t curR=0,curG=0,curB=0;     // ค่า RGB ปัจจุบัน
bool rgbPower=true;               // เปิด/ปิด RGB

/*** Smoke (MQ-2 + LEDs + Buzzer) ***/
#define MQ2_AO           A0
#define LED_SMOKE_AMBER  0   // D3
#define LED_SMOKE_RED    2   // D4
#define BUZZER_PIN       15  // D8

const bool LED_ACTIVE_LOW = false;

inline void ledWrite(uint8_t pin, bool on) {
  if (LED_ACTIVE_LOW)  digitalWrite(pin, on ? LOW  : HIGH);
  else                 digitalWrite(pin, on ? HIGH : LOW);
}
inline void buzzerOn()  { digitalWrite(BUZZER_PIN, HIGH); }
inline void buzzerOff() { digitalWrite(BUZZER_PIN, LOW);  }

/*** Params ***/
float THRESHOLD_CM             = 10.0;  // ระยะตรวจจับ
const float HYSTERESIS_CM      = 5.0;   // กันสวิง
const uint8_t  REQ_CONSEC_NEAR = 2;     // ต้องเจอติดกันกี่ครั้งถึงจะ alert
const uint32_t COOLDOWN_MS     = 500;   // กันแจ้งถี่เกินไป
const uint32_t MEAS_PERIOD_MS  = 50;    // ช่วงเวลาอ่านค่า ultrasonic
const uint32_t ECHO_TIMEOUT_US = 25000; // timeout
const uint32_t PRINT_EVERY_MS  = 1000;

/*** MQ-2 thresholds ***/
int MQ2_ORANGE_RAW = 100;
int MQ2_RED_RAW    = 200;   // ค่าเริ่มต้น threshold ระดับอันตราย
int MQ2_HYST_RAW   = 40;    // hysteresis
const uint32_t MQ2_SAMPLE_MS            = 500;
const uint32_t SMOKE_ALERT_COOLDOWN_MS  = 15000;
const uint32_t BEEP_PERIOD_MS           = 400;
const uint32_t MQ2_WARMUP_MS            = 30000;  // MQ-2 ต้องอุ่นก่อน

/*** States ***/
Ticker ultraTicker;
volatile bool ultraTick=false;

bool ARMED = true;        // เปิด/ปิด ultrasonic
bool alerted = false;     // สถานะแจ้งเตือน ultrasonic
uint8_t  consecNear = 0;  // นับเจอติดต่อ
uint32_t lastAlertMs = 0, lastPrintMs = 0;

uint32_t lastSmokeSample=0,lastSmokeAlert=0,lastBeepToggle=0, bootMs=0;
uint8_t  lastSmokeLevel=0;  // ระดับควันล่าสุด (0,1,2)
bool     beepOn=false;

/*** New Feature States ***/
bool smokeNotify = true;       // เปิด-ปิดการแจ้งเตือนควันผ่าน Blynk
bool buzzerSilenced = false;   // ปิดเสียง buzzer
bool autoFeedEnable = false;   // เปิด-ปิด Auto Feed
int feedHour = -1, feedMinute = -1; // เวลา feed อัตโนมัติ

WidgetRTC rtc;  // RTC widget ของ Blynk

/*** Helpers (RGB) ***/
inline void pwmWrite(uint8_t pin,uint8_t val){
  if(COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
// ตั้งค่า RGB ตาม curR,G,B
void applyRGB(){
  if(!rgbPower){
    pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,0);
  }else{
    pwmWrite(LED_R,curR); pwmWrite(LED_G,curG); pwmWrite(LED_B,curB);
  }
}

/*** Helpers (Servo) ***/
// เช็คว่า servo พร้อมทำงานไหม
bool canStartServoNow(){
  return (!servoBusy) && (millis() - servoLastDoneMs >= SERVO_LOCKOUT_MS);
}
// สั่งเปิด servo
void startServoPulseOpen(){
  if(!canStartServoNow()) return;
  servoBusy = true;
  servoActionStart = millis();
  foodServo.writeMicroseconds(SERVO_OPEN_US);
  Serial.println("[Servo] OPEN (us)");
}
// ปิด servo หลังครบเวลา
void processServoPulse(){
  if(servoBusy && (millis()-servoActionStart >= SERVO_PULSE_MS)){
    foodServo.writeMicroseconds(SERVO_CLOSED_US);
    servoBusy = false;
    servoLastDoneMs = millis();
    Serial.println("[Servo] CLOSE (us)");
  }
}

/*** Blynk handlers ***/
// V4 armed เปิด/ปิด ultrasonic
BLYNK_WRITE(V4){ ARMED = (param.asInt()==1); }
// V5 feed button (Manual Feed)
BLYNK_WRITE(V5){ if(param.asInt()==1) startServoPulseOpen(); }
// V8/V9/V10 zeRGBa
BLYNK_WRITE(V8){ curR = constrain(param.asInt(),0,255); applyRGB(); }
BLYNK_WRITE(V9){ curG = constrain(param.asInt(),0,255); applyRGB(); }
BLYNK_WRITE(V10){curB = constrain(param.asInt(),0,255); applyRGB(); }
// V12 RGB Power
BLYNK_WRITE(V12){ rgbPower = (param.asInt()==1); applyRGB(); }
// V13 smoke notify
BLYNK_WRITE(V13){ smokeNotify = (param.asInt() == 1); }
// V14 buzzer silence
BLYNK_WRITE(V14){ buzzerSilenced = (param.asInt() == 1); if(buzzerSilenced){ buzzerOff(); beepOn=false; } }
// V15 feed hour
BLYNK_WRITE(V15){ feedHour = param.asInt(); autoFeedEnable = (feedHour >= 0 && feedMinute >= 0); }
// V16 feed minute
BLYNK_WRITE(V16){ feedMinute = param.asInt(); autoFeedEnable = (feedHour >= 0 && feedMinute >= 0); }
// V17 smoke red threshold
BLYNK_WRITE(V17) {
  int val = param.asInt();
  if(val > 0 && val <= 1023) {
    MQ2_RED_RAW = val;
    Serial.printf("[Blynk] MQ2_RED_RAW set = %d\n", MQ2_RED_RAW);
  }
}

BLYNK_CONNECTED(){
  rtc.begin();
  Blynk.syncVirtual(V4,V8,V9,V10,V12,V13,V14,V15,V16,V17);
}

/*** Ultrasonic ***/
void ultraTickCb(){ ultraTick = true; }     // callback ของ Ticker
float readUltrasonicOnceCM(){               // อ่านค่าระยะ cm
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if(dur==0) return NAN;
  return (float)dur * 0.01715f;
}

/*** Auto Feed : ถ้าเวลา (h,m) ตรงกับ feedHour/feedMinute → ให้อาหาร***/
void checkAutoFeed(){
  static int lastMinute = -1;
  if(autoFeedEnable){
    int h = hour();
    int m = minute();
    if(m != lastMinute){
      lastMinute = m;
      if(h == feedHour && m == feedMinute){
        Serial.println("[AutoFeed] Feeding now...");
        startServoPulseOpen();
      }
    }
  }
}

/*** Setup : กำหนด pin, init servo, WiFi, Blynk, RGB, ultrasonic ticker***/
void setup(){
  pinMode(TRIG_PIN,OUTPUT); pinMode(ECHO_PIN,INPUT);
  pinMode(LED_R,OUTPUT); pinMode(LED_G,OUTPUT); pinMode(LED_B,OUTPUT);
  pinMode(LED_SMOKE_AMBER,OUTPUT);
  pinMode(LED_SMOKE_RED,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  ledWrite(LED_SMOKE_AMBER, false);
  ledWrite(LED_SMOKE_RED,   false);
  buzzerOff();

  analogWriteRange(255);
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  ultraTicker.attach_ms(MEAS_PERIOD_MS, ultraTickCb);

  foodServo.attach(SERVO_PIN, 500, 2500);
  foodServo.writeMicroseconds(SERVO_CLOSED_US);

  applyRGB();
  bootMs = millis();
  Serial.println("[MQ-2] Warm-up ~30s...");
  Blynk.virtualWrite(V17, MQ2_RED_RAW);
}

/*** Loop ***/
void loop(){
  Blynk.run();
  processServoPulse();  // ดูแล servo
  checkAutoFeed();      // ตรวจสอบ auto feed

  // -------- Ultrasonic --------
  if(ultraTick){
    ultraTick = false;
    float d = readUltrasonicOnceCM();
    uint32_t now = millis();
    if(!isnan(d)) Blynk.virtualWrite(V2, d);

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

  // -------- MQ-2 --------
  uint32_t now = millis();
  bool mq2Warmup = (now - bootMs < MQ2_WARMUP_MS);

  if(now - lastSmokeSample >= MQ2_SAMPLE_MS){
    lastSmokeSample = now;
    int raw = analogRead(MQ2_AO);
    Blynk.virtualWrite(V6, raw);

    uint8_t lvl = lastSmokeLevel;
    if(!mq2Warmup){
      if      (lvl == 0) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw >= MQ2_ORANGE_RAW) lvl = 1; }
      else if (lvl == 1) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw <= MQ2_ORANGE_RAW - MQ2_HYST_RAW) lvl = 0; }
      else /*2*/        { if(raw <= MQ2_RED_RAW - MQ2_HYST_RAW) lvl = 1; }
    }else{
      lvl = 0;
    }

    if(lvl != lastSmokeLevel){
      lastSmokeLevel = lvl;
      Blynk.virtualWrite(V7, lvl);

      ledWrite(LED_SMOKE_AMBER, (lvl >= 1));
      ledWrite(LED_SMOKE_RED,   (lvl >= 2));

      if(lvl < 2){ buzzerOff(); beepOn = false; }

      if(lvl == 2){
        if(smokeNotify && (now - lastSmokeAlert >= SMOKE_ALERT_COOLDOWN_MS)){
          lastSmokeAlert = now;
          Blynk.logEvent("smoke_alert", "ตรวจพบควันระดับสูง (RED)");
        }
      }
    }
  }

  // Beep Logic เมื่อ RED และทั้ง notify เปิดอยู่ + ไม่ silence
  if(!mq2Warmup && lastSmokeLevel == 2 && smokeNotify && !buzzerSilenced){
    if(now - lastBeepToggle >= BEEP_PERIOD_MS){
      lastBeepToggle = now;
      beepOn = !beepOn;
      if (beepOn) buzzerOn(); else buzzerOff();
    }
  }else{
    if (beepOn){ buzzerOff(); beepOn=false; }
  }
}
