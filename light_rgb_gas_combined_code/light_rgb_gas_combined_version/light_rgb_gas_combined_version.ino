// ===== RGB with Button (debounce) + PIR smooth =====
#define LED_R 9
#define LED_G 10
#define LED_B 11
#define BTN   7

#define LED_PIR 3
#define PIR     6

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
const unsigned long LED_PIR_HOLD_MS = 5000;
unsigned long lastMotionMs = 0;
bool ledPirOn = false;

// ===== MQ-2 + LEDs + Buzzer =====
#define BUZZER_PIN  13

#define GAS_LED1  2
#define GAS_LED2  4
#define GAS_LED3  5
#define GAS_LED4  8
#define GAS_LED5  12   
#define PIN_GAS   A3   // MQ-2 AO

void setup() {
  // RGB + PIR
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);

  pinMode(LED_PIR, OUTPUT);
  pinMode(PIR, INPUT);

  showIdx();
  digitalWrite(LED_PIR, LOW);

  // Gas LEDs + buzzer
  pinMode(GAS_LED1, OUTPUT);
  pinMode(GAS_LED2, OUTPUT);
  pinMode(GAS_LED3, OUTPUT);
  pinMode(GAS_LED4, OUTPUT);
  pinMode(GAS_LED5, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(9600);
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

  // === PIR LED ===
  unsigned long now = millis();
  int pirState = digitalRead(PIR);

  if (pirState == HIGH) {
    ledPirOn = true;
    lastMotionMs = now;
    digitalWrite(LED_PIR, HIGH);
  } else {
    if (ledPirOn && (now - lastMotionMs >= LED_PIR_HOLD_MS)) {
      ledPirOn = false;
      digitalWrite(LED_PIR, LOW);
    }
  }

  // === MQ-2 Gas Sensor ===
  int sensorValue = analogRead(PIN_GAS);
  int value = map(sensorValue, 300, 1000, 0, 100);
  if (value < 0) value = 0;
  if (value > 100) value = 100;

  // Gas level LEDs
  digitalWrite(GAS_LED1, HIGH);
  digitalWrite(GAS_LED2, value >= 20 ? HIGH : LOW);
  digitalWrite(GAS_LED3, value >= 40 ? HIGH : LOW);
  digitalWrite(GAS_LED4, value >= 60 ? HIGH : LOW);
  digitalWrite(GAS_LED5, value >= 80 ? HIGH : LOW);

  // Buzzer
  if (value >= 50) {
    int frequency = map(value, 0, 100, 1500, 2500);
    tone(BUZZER_PIN, frequency, 250);
  } else {
    noTone(BUZZER_PIN);
  }

  Serial.print("Gas raw = ");
  Serial.print(sensorValue);
  Serial.print("  % = ");
  Serial.println(value);

  delay(100);
}
