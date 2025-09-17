// ===== RGB (many colors) with Debounce & Start-Off =====
// ปุ่ม BTN คุม RGB (วนสี)
// PIR คุม LED ธรรมดา (LED2) แบบ smooth: ติดทันทีเมื่อขยับ, ดับหลังไม่มีขยับต่อเนื่อง >5 วิ

#define LED_R 9
#define LED_G 10
#define LED_B 11
#define BTN   7

#define LED2 3
#define PIR  6

const bool COMMON_ANODE = false;

// ---- Debounce BTN ----
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;
int lastReading = HIGH;
int stableState = HIGH;

// ---- RGB colors ----
struct RGB { uint8_t r,g,b; };
RGB COLORS[] = {
  {0,0,0},{255,0,0},{0,255,0},{0,0,255},
  {255,255,0},{0,255,255},{255,0,255},{255,255,255},
  {255,128,0},{128,0,255},{255,64,64},{0,128,64}
};
const uint8_t NUM_COLORS = sizeof(COLORS)/sizeof(COLORS[0]);
uint8_t idx = 0;

inline void setColor(uint8_t r, uint8_t g, uint8_t b) {
  if (COMMON_ANODE) { r = 255-r; g = 255-g; b = 255-b; }
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}
void showIdx(){ setColor(COLORS[idx].r, COLORS[idx].g, COLORS[idx].b); }
void nextColor(){ idx=(idx+1)%NUM_COLORS; showIdx(); }

// ---- PIR control ----
const unsigned long LED2_HOLD_MS = 5000;
unsigned long lastMotionMs = 0;
bool led2On = false;

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);

  pinMode(LED2, OUTPUT);
  pinMode(PIR, INPUT);

  showIdx();
  digitalWrite(LED2, LOW);
}

void loop() {
  // === Button RGB ===
  int reading = digitalRead(BTN);
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {
        nextColor();
      }
    }
  }
  lastReading = reading;

  // === PIR LED2 ===
  unsigned long now = millis();
  int pirState = digitalRead(PIR);

  if (pirState == HIGH) {
    // มีการเคลื่อนไหว -> เปิดไฟทันที และรีเซ็ตเวลา
    led2On = true;
    lastMotionMs = now;
    digitalWrite(LED2, HIGH);
  } else {
    // ไม่มีการเคลื่อนไหว -> เช็คว่าเลย 5 วิหรือยัง
    if (led2On && (now - lastMotionMs >= LED2_HOLD_MS)) {
      led2On = false;
      digitalWrite(LED2, LOW);
    }
  }
}
