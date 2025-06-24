const int sensorPin = 3;    // KY-002 signal pin
const int ledPin = 10;      // Red LED pin

void setup() {
  pinMode(sensorPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);       
}

void loop() {
  int vibration = digitalRead(sensorPin);

  if (vibration == LOW) {   
    digitalWrite(ledPin, HIGH);
    Serial.println("Earthquake Detected!");
    delay(1000);            
  } else {
    digitalWrite(ledPin, LOW);
  }

  // delay(50);               
}
