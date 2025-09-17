#include <Servo.h>

int servoButtonPin = D7;   // Food Gate Servo
Servo servoButton;

int buttonPin = D3;        // Push Button Feeder 
int pirPin = D1;           // PIR Sensor
int buzzerPIR = D2;        // Buzzer PIR

void setup() {
  servoButton.attach(servoButtonPin);
  servoButton.write(0);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(pirPin, INPUT);
  pinMode(buzzerPIR, OUTPUT);
}

void loop() {
  // Push Button Feeder
  if (digitalRead(buttonPin) == LOW) {
    servoButton.write(180);
    delay(150);
    servoButton.write(0);
    delay(200);
    while(digitalRead(buttonPin) == LOW);  // wait release
  }

  // PIR Sensor
  if (digitalRead(pirPin) == HIGH) {
    digitalWrite(buzzerPIR, HIGH);
    delay(1000);
    digitalWrite(buzzerPIR, LOW);
  }

  delay(100);
}
