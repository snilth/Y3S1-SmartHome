#include <Servo.h>

Servo servo;
int servoPin = 8;       // Servo
int buttonPin = 9;      // Push Button
int trigPin = 5;        // HC-SR04 Trig
int echoPin = 6;        // HC-SR04 Echo
int buzzerPin = 7;      // Buzzer

long distanceThreshold = 20; // cm, ระยะใกล้ให้ buzzer ดัง

void setup() {
  servo.attach(servoPin);
  servo.write(0);                    // start at 0°
  pinMode(buttonPin, INPUT_PULLUP);  // Push Button internal pull-up
  pinMode(trigPin, OUTPUT);          // Trig output
  pinMode(echoPin, INPUT);           // Echo input
  pinMode(buzzerPin, OUTPUT);        // Buzzer output
  Serial.begin(9600);                // debug
}

long readDistanceCM() {
  // ส่งสัญญาณ ultrasonic
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // อ่าน Echo
  long duration = pulseIn(echoPin, HIGH);

  // แปลงเป็นระยะทาง cm
  long distance = duration * 0.034 / 2;
  return distance;
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

  // --- Ultrasonic Sensor ---
  long distance = readDistanceCM();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance <= distanceThreshold) {
    digitalWrite(buzzerPin, HIGH);       // Buzzer alarm
    delay(1000);
    digitalWrite(buzzerPin, LOW);
  }

  delay(100); // read every 100ms
}
