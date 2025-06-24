const int soundPin = A0;      // Grove sound sensor connected to A0
const int ledPin = 10;        // LED connected to digital pin 10
const int threshold = 100;    // Threshold value (normally loud sounds have high value but sometimes the sensor works weirdly in opposite way)


void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int soundLevel = analogRead(soundPin);
  Serial.println(soundLevel);  

  if (soundLevel < threshold ) {
    digitalWrite(ledPin, HIGH);
    Serial.println("Loud noise detected!");
  } 
  else if (soundLevel > threshold ) {
    digitalWrite(ledPin, LOW);
  }

  delay(50);  // For stability
}
