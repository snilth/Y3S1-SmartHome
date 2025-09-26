/***** MQ-2 + LEDs Test (ESP8266) *****/
#define MQ2_AO           A0   // Analog input
#define LED_SMOKE_AMBER  0    // D3
#define LED_SMOKE_RED    2    // D4

// ปรับตรงนี้ให้ตรงกับการต่อจริง
const bool LED_AMBER_ACTIVE_LOW = false;   // ถ้าเป็น LED ภายนอกธรรมดา → false
const bool LED_RED_ACTIVE_LOW   = false;   // ถ้าใช้ D4 on-board LED → true

// ค่า threshold คร่าว ๆ (ต้องปรับเองจากการทดลอง)
int MQ2_ORANGE_RAW = 200;   // เริ่มมีควัน
int MQ2_RED_RAW    = 600;   // ควันเยอะ
int MQ2_HYST_RAW   = 40;    // hysteresis กันไฟกระพริบ

uint8_t lastSmokeLevel = 0;  // 0=OK, 1=AMBER, 2=RED

inline void ledWrite(uint8_t pin, bool on, bool activeLow){
  if(activeLow) digitalWrite(pin, on ? LOW : HIGH);
  else          digitalWrite(pin, on ? HIGH : LOW);
}

void setup() {
  pinMode(LED_SMOKE_AMBER, OUTPUT);
  pinMode(LED_SMOKE_RED, OUTPUT);

  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== MQ-2 Smoke Test Start ===");
  Serial.println("Warm-up sensor ~30s...");
}

void loop() {
  static uint32_t lastSample = 0;
  uint32_t now = millis();

  if(now - lastSample >= 500){   // อ่านทุก 0.5s
    lastSample = now;

    int raw = analogRead(MQ2_AO);   // 0..1023
    uint8_t lvl = lastSmokeLevel;

    // hysteresis เล็กน้อย
    if      (lvl == 0) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw >= MQ2_ORANGE_RAW) lvl = 1; }
    else if (lvl == 1) { if(raw >= MQ2_RED_RAW) lvl = 2; else if(raw <= MQ2_ORANGE_RAW - MQ2_HYST_RAW) lvl = 0; }
    else /*2*/        { if(raw <= MQ2_RED_RAW - MQ2_HYST_RAW) lvl = 1; }

    if(lvl != lastSmokeLevel){
      lastSmokeLevel = lvl;
      if(lvl == 0) Serial.printf("Smoke=OK    raw=%d\n", raw);
      if(lvl == 1) Serial.printf("Smoke=AMBER raw=%d\n", raw);
      if(lvl == 2) Serial.printf("Smoke=RED   raw=%d\n", raw);
    }

    // อัปเดต LED
    ledWrite(LED_SMOKE_AMBER, (lvl >= 1), LED_AMBER_ACTIVE_LOW);
    ledWrite(LED_SMOKE_RED,   (lvl >= 2), LED_RED_ACTIVE_LOW);
  }
}
