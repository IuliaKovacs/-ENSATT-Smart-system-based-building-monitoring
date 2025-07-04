#include "people_counting.h"

UltraSonicDistanceSensor enter_hcsr04(ENTER_TRIG_PIN, ENTER_ECHO_PIN);
UltraSonicDistanceSensor leave_hcsr04(LEAVE_TRIG_PIN, LEAVE_ECHO_PIN);

State currentState = IDLE;
uint16_t peopleCount = 0;
uint32_t lastTransitionTime = 0;
float averageValueRead = 0;
bool presenceDetected = false;

void initInfraredCamera() {
  Wire.begin();
  // Wire.setClock(400000);
  Serial.println("INFRA INIT");
}

void initUltrasonicSensors() {
  pinMode(ENTER_TRIG_PIN, OUTPUT);
  pinMode(ENTER_ECHO_PIN, INPUT);

  pinMode(LEAVE_TRIG_PIN, OUTPUT);
  pinMode(LEAVE_ECHO_PIN, INPUT);
  Serial.println("ULTR INIT");
}


void infraredCameraCheck() {
  readMatrix();

  if(averageValueRead > HUMAN_DETECTION_THRESHOLD){
    presenceDetected = true;
    Serial.println("PERS D");
  } 
}

uint16_t readRegister(uint16_t address) {
  Wire.beginTransmission(IR_CAMERA_ADDR);
  
  Wire.write(address >> 8);
  Wire.write(address & 0xff);

  Wire.endTransmission(false);

  Wire.requestFrom(IR_CAMERA_ADDR, 2);


  if (Wire.available() >= 2) 
    return (Wire.read() << 8) | Wire.read();

  return -1;
}

void readMatrix() {
  averageValueRead = 0;

  for (uint8_t y = 0; y < 24; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      uint16_t pixelAddr = 0x400 + x + y * 32;
      int16_t pixelValue = readRegister(pixelAddr);
      averageValueRead += pixelValue;

      if (x != 0 && y != 0) averageValueRead /= 2;
    }
  }
}


void counterSysSTM() {
  int enterDistanceValue = enter_hcsr04.measureDistanceCm();
  int leaveDistanceValue = leave_hcsr04.measureDistanceCm();

  switch (currentState) {
      case IDLE:
        // Serial.println("IDLE");
        presenceDetected = false;
        if (enterDistanceValue < DISTANCE_THRESHOLD && leaveDistanceValue != -1) {
          currentState = ENTER_DETECT;
          lastTransitionTime = millis();
        } else if (leaveDistanceValue < DISTANCE_THRESHOLD && leaveDistanceValue != -1) {
          currentState = LEAVE_DETECT;
          lastTransitionTime = millis();
        }
        break;

      case ENTER_DETECT:
        // Serial.println("ENTER_DETECT");
        infraredCameraCheck();
        if (millis() - lastTransitionTime > 2000) {
          currentState = IDLE;
        } else if (presenceDetected && (leaveDistanceValue < DISTANCE_THRESHOLD)) {
          currentState = ENTER_COMPLETE;
        }
        break;

      case LEAVE_DETECT:
        // Serial.println("LEAVE_DETECT");
        infraredCameraCheck();
        if (millis() - lastTransitionTime > 2000) {
          currentState = IDLE;
        } else if (presenceDetected && (enterDistanceValue < DISTANCE_THRESHOLD)) {
          currentState = LEAVE_COMPLETE;
        }
        break;

      case ENTER_COMPLETE:
        if((leaveDistanceValue > DISTANCE_THRESHOLD) || (leaveDistanceValue == -1)){
          peopleCount++;
          Serial.print("P IN:");
          Serial.println(peopleCount);

          currentState = IDLE;
        }
        break;

      case LEAVE_COMPLETE:
        if(peopleCount > 0){
          if ((enterDistanceValue > DISTANCE_THRESHOLD) || (enterDistanceValue == -1)) {
            peopleCount--;
            Serial.print("P OUT: ");
            Serial.println(peopleCount);

            currentState = IDLE;
          }
        } else {
          Serial.println("SEN E");
          currentState = IDLE;
        }
        break;
    }
}