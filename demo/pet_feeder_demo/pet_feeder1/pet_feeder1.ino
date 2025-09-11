#include <Servo.h>

Servo servo;
int servoPin = 8;       // Servo
int buttonPin = 9;      // Push Button
int pirPin = 6;         // PIR Sensor
int buzzerPin = 7;      // Buzzer

void setup() {
  servo.attach(servoPin);
  delay(500);                    // รอสักครู่ก่อนส่งสัญญาณ PWM
  servo.write(0);                // start at 0°
  pinMode(buttonPin, INPUT_PULLUP);  // Push Button internal pull-up
  pinMode(pirPin, INPUT);            // PIR Sensor input
  pinMode(buzzerPin, OUTPUT);        // Buzzer output
}

void loop() {
  // --- Push Button ---
  if (digitalRead(buttonPin) == LOW) {  	// press button
    servo.write(90);                     	// rotate Servo
    delay(2000);                         	// wait 2 secs
    servo.write(0);                      	// rotate back Servo
    delay(200);                           	// debounce
    while(digitalRead(buttonPin) == LOW); 	// wait for button release
  }

  // --- PIR Sensor ---
  if (digitalRead(pirPin) == HIGH) {     // Pet in PIR detect zone
    digitalWrite(buzzerPin, HIGH);       // Buzzer alarm
    delay(1000);                         // 1 sec
    digitalWrite(buzzerPin, LOW);        // close Buzzer
  }

  delay(100); // read PIR every 100ms
}
