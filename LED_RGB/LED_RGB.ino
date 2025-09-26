#define BLYNK_TEMPLATE_ID "TMPL6CQA_FLZL"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN    "pCRjqSzxGIpAvDxbAtfKaiGGEpf8MU5T"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Wi-Fi
char ssid[] = "iPhone k";
char pass[] = "Khim.2005";

// ===== RGB (many colors) with Debounce & Start-Off =====
// โปรแกรมนี้ควบคุม LED RGB ด้วยปุ่มกด 1 ปุ่ม
// กดแต่ละครั้งจะเปลี่ยนสีตามลำดับ (รวมถึงสถานะ "ดับ")
// ใช้การ Debounce เพื่อป้องกันการอ่านปุ่มซ้ำจากการเด้งของสัญญาณ

// --------------------- การกำหนดขา ---------------------
// RGB LED
const int LED_R = D1;
const int LED_G = D2;
const int LED_B = D7;
const bool COMMON_ANODE = false;

// --------------------- ชนิดของ LED ---------------------
// กำหนดชนิดของ LED RGB
// ถ้าเป็น Common Cathode (CC) → false  (ขากลางต่อ GND)
// ถ้าเป็น Common Anode (CA)   → true   (ขากลางต่อ +5V)
const bool COMMON_ANODE = false;   // <-- เปลี่ยนเป็น true ถ้าใช้ CA

// --------------------- ตัวแปร Debounce ---------------------
// ใช้ตรวจจับการเด้งของปุ่มกด
unsigned long lastDebounceTime = 0;     // เวลาที่อ่านปุ่มล่าสุด
const unsigned long DEBOUNCE_MS = 50;  // หน่วงเวลา debounce 50 มิลลิวินาที
int lastReading = HIGH;                // ค่าที่อ่านล่าสุด (เริ่ม HIGH เพราะ INPUT_PULLUP)
int stableState = HIGH;                // ค่าปุ่มที่เสถียรล่าสุด

// --------------------- ตารางสี ---------------------
// สร้าง struct เก็บค่า RGB
struct RGB { uint8_t r,g,b; };

// กำหนดสีที่ต้องการไล่ (0–255)
// เริ่มด้วย Off (ดับ) ตามด้วย Red, Green, Blue, ...
RGB COLORS[] = {
  {  0,   0,   0}, // 0: Off
  {255,   0,   0}, // 1: Red
  {  0, 255,   0}, // 2: Green
  {  0,   0, 255}, // 3: Blue
  {255, 255,   0}, // 4: Yellow
  {  0, 255, 255}, // 5: Cyan
  {255,   0, 255}, // 6: Magenta
  {255, 255, 255}, // 7: White
  {255, 128,   0}, // 8: Orange
  {128,   0, 255}, // 9: Purple
  {255,  64,  64}, // 10: Pink-ish
  {  0, 128,  64}  // 11: Teal-ish
};
const uint8_t NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]); // นับจำนวนสี
uint8_t idx = 0; // index ของสีปัจจุบัน (เริ่มที่ 0 = Off)

// --------------------- ฟังก์ชันควบคุมสี ---------------------
// ตั้งค่า RGB ตามค่าที่กำหนด
inline void setColor(uint8_t r, uint8_t g, uint8_t b) {
  // ถ้าเป็น Common Anode ต้องกลับตรรกะ (inverted PWM)
  if (COMMON_ANODE) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(LED_R, r); // ส่ง PWM ไปที่ LED สีแดง
  analogWrite(LED_G, g); // ส่ง PWM ไปที่ LED สีเขียว
  analogWrite(LED_B, b); // ส่ง PWM ไปที่ LED สีน้ำเงิน
}

// แสดงสีตาม index ปัจจุบัน
void showIdx() {
  setColor(COLORS[idx].r, COLORS[idx].g, COLORS[idx].b);
}

// เลื่อนไปสีถัดไป (วนกลับไปเริ่มใหม่เมื่อสุด)
void nextColor() {
  idx = (idx + 1) % NUM_COLORS; // เพิ่ม index และวนกลับด้วย mod
  showIdx(); // แสดงสีตาม index
}

// --------------------- การตั้งค่าเริ่มต้น ---------------------
void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BTN,   INPUT_PULLUP); // ใช้ internal pull-up → ปล่อย = HIGH, กด = LOW

  // เริ่มต้นด้วย "ดับ" (idx = 0)
  showIdx();
}

// --------------------- ลูปหลัก ---------------------
void loop() {
  int reading = digitalRead(BTN); // อ่านสถานะปุ่ม

  // ถ้าค่าที่อ่านเปลี่ยนไปจากครั้งก่อน → รีเซ็ตเวลา debounce
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  // ถ้าผ่านเวลา debounce แล้ว ให้ตรวจจับการเปลี่ยนสถานะจริง
  if (millis() - lastDebounceTime > DEBOUNCE_MS) {
    // ถ้าค่าที่อ่านต่างจากค่าที่เสถียรล่าสุด
    if (reading != stableState) {
      stableState = reading; // อัปเดตสถานะเสถียรใหม่
      // ตรวจจับ "ขอบตก" (HIGH → LOW) หมายถึงการกดปุ่ม
      if (stableState == LOW) {
        nextColor(); // เปลี่ยนไปสีถัดไป
      }
    }
  }
  lastReading = reading; // เก็บค่าปัจจุบันไว้เทียบรอบหน้า
}
