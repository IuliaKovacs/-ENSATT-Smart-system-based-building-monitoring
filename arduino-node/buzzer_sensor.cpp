#include "buzzer_sensor.h"

#define BUZZER_PIN 8

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
}

void playAlarmSound() {
  tone(BUZZER_PIN, 1000);  
  delay(1000);
  tone(BUZZER_PIN, 2000);  
  delay(1000);
  noTone(BUZZER_PIN);      
  delay(500);
}