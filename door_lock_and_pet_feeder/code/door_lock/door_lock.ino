#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Servo.h>

// --- Password Lock ---
#define PWnmbrLength 5
char Data[PWnmbrLength];
char Master[PWnmbrLength] = "1150"; // password
byte data_count = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD address=0x27

int servoLockPin = 12;  // Servo for lock
Servo servoLock;

int buzzerLock = 10;    // Buzzer for password lock

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// using PCF8574
byte rowPins[ROWS] = {0, 1, 2, 3};  // R1:P0 R2:P1 R3:P2 R4:P3
byte colPins[COLS] = {4, 5, 6, 7};  // C1:P4 C2:P5 C3:P6 C4:P7

#define KEYPAD_ADDR 0x20  
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  keypad.begin();

  // --- Servo Lock ---
  servoLock.attach(servoLockPin);
  servoLock.write(0);  // lock position
  pinMode(buzzerLock, OUTPUT);

}

void loop() {
  // --- Password Lock ---
  char key = keypad.getKey();
  if (key) {
    Data[data_count] = key;
    lcd.setCursor(data_count, 1);
    lcd.print('*');
    data_count++;
  }

  if (data_count == PWnmbrLength - 1) {
    Data[data_count] = '\0';
    lcd.clear();
    if (!strcmp(Data, Master)) {
      lcd.print("Unlocking...");
      
      // --- Buzzer success sound ---
      digitalWrite(buzzerLock, HIGH);
      delay(800);               
      digitalWrite(buzzerLock, LOW);

      servoLock.write(90);
      delay(5000);
      servoLock.write(0);
    } else {
      lcd.print("Incorrect!");
      // --- Buzzer error sound ---
      for (int i = 0; i < 2; i++) {   
        digitalWrite(buzzerLock, HIGH);
        delay(150);                   
        digitalWrite(buzzerLock, LOW);
        delay(150);                   
      }
    }
    clearData();
    lcd.clear();
    lcd.print("Enter Password:");
  }
  delay(100);
}

void clearData() {
  for (byte i = 0; i < PWnmbrLength; i++) Data[i] = '\0';
  data_count = 0;
}