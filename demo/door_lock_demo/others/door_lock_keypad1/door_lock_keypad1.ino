#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

#define PWnmbrLength 5
char Data[PWnmbrLength];
char Master[PWnmbrLength] = "1150";	// password
byte data_count = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);	// address=0x27, 16x2 LCD

int servoPin = 12;  // Servo
Servo servo;

int buzzer = 10;    // Buzzer
int LEDred = 9;     // Red LED --> incorrect password
int LEDgreen = 11;  // Green LED --> correct password

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 2, 3, 4, 5 };   // Keypad Rows
byte colPins[COLS] = { 6, 7, 8, 13 };  // Keypad Cols

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");

  servo.attach(servoPin);
  servo.write(0);  // start lock

  pinMode(buzzer, OUTPUT);
  pinMode(LEDred, OUTPUT);
  pinMode(LEDgreen, OUTPUT);
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    Data[data_count] = key;
    lcd.setCursor(data_count, 1);  // row=1
    lcd.print('*');                // display * instead of number
    data_count++; 
  }

  if (data_count == PWnmbrLength - 1) {
    Data[data_count] = '\0'; 
    lcd.clear();

    if (!strcmp(Data, Master)) {
      lcd.print("Unlocking...");
      digitalWrite(LEDgreen, HIGH);

      servo.write(180);  // rotate 180° immediately
      delay(3000);       // wait 3 secs
      servo.write(0);    // lock

      digitalWrite(LEDgreen, LOW);
    } else {
      lcd.print("Incorrect!");
      digitalWrite(LEDred, HIGH);
      digitalWrite(buzzer, HIGH);
      delay(1000);
      digitalWrite(buzzer, LOW);
      digitalWrite(LEDred, LOW);
    }

    clearData();
    lcd.clear();
    lcd.print("Enter Password:");
  }
}


void clearData() {
  for (byte i = 0; i < PWnmbrLength; i++) Data[i] = '\0';
  data_count = 0;
}
