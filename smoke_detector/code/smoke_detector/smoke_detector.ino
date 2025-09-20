#define BUZZER_PIN  13

// Pin 13 has an LED connected on most Arduino boards.
#define PIN_LED_1 	2
#define PIN_LED_2 	4
#define PIN_LED_3 	5
#define PIN_LED_4 	8
#define PIN_LED_5 	12
#define PIN_GAS 	A3 // MQ-2

// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  pinMode(PIN_LED_3, OUTPUT);
  pinMode(PIN_LED_4, OUTPUT);
  pinMode(PIN_LED_5, OUTPUT);
  Serial.begin(9600);
}

// the loop routine runs over and over again forever:
void loop() {
  
  long frequency;
  
  int value = map(analogRead(PIN_GAS), 300, 750, 0, 100);
  digitalWrite(PIN_LED_1, HIGH);
  digitalWrite(PIN_LED_2, value >= 20 ? HIGH : LOW);
  digitalWrite(PIN_LED_3, value >= 40 ? HIGH : LOW);
  digitalWrite(PIN_LED_4, value >= 60 ? HIGH : LOW);
  digitalWrite(PIN_LED_5, value >= 80 ? HIGH : LOW);
  
  frequency = map(value, 0, 1023, 1500, 2500);
  // We make the pin with the buzzer “vibrate”, i.e. produce sound
  // (in English: tone) at the given frequency for 20 milliseconds.
  // On the next passes of the loop, tone will be called again and again,
  // and in practice we will hear a continuous sound whose pitch
  // depends on the amount of light falling on the photoresistor

	if (value>=50){
	   tone(BUZZER_PIN, frequency, 250);
    }
      
  Serial.println(value);
  delay(250); // wait for a quarter second
}