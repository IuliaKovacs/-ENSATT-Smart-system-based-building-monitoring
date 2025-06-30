#include "people_counting.h"

UltraSonicDistanceSensor enter_hcsr04(ENTER_TRIG_PIN, ENTER_ECHO_PIN);
UltraSonicDistanceSensor leave_hcsr04(LEAVE_TRIG_PIN, LEAVE_ECHO_PIN);


State currentState = IDLE;
int peopleCount = 0;
unsigned long lastTransitionTime = 0;
float averageValueRead = 0;
bool presenceDetected = false;


void initInfraredCamera() {
  Wire.begin();        // join I2C bus (address optional for master)
  Wire.setClock(400000);
}

void initUltrasonicSensors() {
  pinMode(ENTER_TRIG_PIN, OUTPUT);
  pinMode(ENTER_ECHO_PIN, INPUT);

  pinMode(LEAVE_TRIG_PIN, OUTPUT);
  pinMode(LEAVE_ECHO_PIN, INPUT);
  Serial.println("Ultrasonic sensors were intialized");
}


void infraredCameraCheck() {
  readMatrix();

  if(averageValueRead > HUMAN_DETECTION_THRESHOLD){
    presenceDetected = true;
    Serial.println(">>> PERSON or ANIMAL DETECTED!");
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

  return SENSOR_ERROR;
}

uint16_t writeRegister(uint16_t address, uint16_t value) {
  Wire.beginTransmission(IR_CAMERA_ADDR);
  
  Wire.write(address >> 8);
  Wire.write(address & 0xff);

  Wire.write(value >> 8);
  Wire.write(value & 0xff);

  uint8_t error = Wire.endTransmission();
  return error;
}

void readMatrix() {

  for (uint8_t y = 0; y < 24; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      uint16_t pixelAddr = 0x400 + x + y * 32;
      int16_t pixelValue = readRegister(pixelAddr);
      averageValueRead += pixelValue;

      if (x != 0 && y != 0) averageValueRead /= 2;

      // Serial.print("Pixel (");
      // Serial.print(x);
      // Serial.print("; ");
      // Serial.print(y);
      // Serial.print("): ");
      // Serial.println(pixelValue);
    }
  }


  // Serial.print("Average Temperature: ");
  // Serial.println(averageValueRead);
}


void counterSysSTM() {
  int enterDistanceValue = enter_hcsr04.measureDistanceCm();
  int leaveDistanceValue = leave_hcsr04.measureDistanceCm();

  // Serial.println(currentState);

   switch (currentState) {
      case IDLE:
      Serial.println("IDLE");
        presenceDetected = false;
        if (enterDistanceValue < DISTANCE_THRESHOLD && enterDistanceValue != -1) {  // Check that the distance value is valid before using it.  
                                                                                    // Some sensors return -1 to indicate an invalid or failed reading.
                                                                                    // Without this check, the condition could trigger incorrectly.
          currentState = ENTER_DETECT;
          lastTransitionTime = millis();
        } else if (leaveDistanceValue < DISTANCE_THRESHOLD && leaveDistanceValue != -1) {
          currentState = LEAVE_DETECT;
          lastTransitionTime = millis();
        }
        break;

      case ENTER_DETECT:
      Serial.println("ENTER_DETECT");
        infraredCameraCheck();
        if (millis() - lastTransitionTime > 2000) { // Timeout di 2 secondi
          currentState = IDLE;
        } else if (presenceDetected && (leaveDistanceValue < DISTANCE_THRESHOLD && leaveDistanceValue != -1)) {
          currentState = ENTER_COMPLETE;
        }
        break;

      case LEAVE_DETECT:
      Serial.println("LEAVE_DETECT");
        infraredCameraCheck();
        if (millis() - lastTransitionTime > 2000) { // Timeout di 2 secondi
          currentState = IDLE;
        } else if (presenceDetected && (enterDistanceValue < DISTANCE_THRESHOLD && enterDistanceValue != -1)) {
          currentState = LEAVE_COMPLETE;
        }
        break;

      case ENTER_COMPLETE:
      Serial.println("ENTER_COMPLETE");
        if ((leaveDistanceValue > DISTANCE_THRESHOLD) || (leaveDistanceValue == -1)) {
          peopleCount++;
          Serial.println("Persona ENTRATA. Totale: " + String(peopleCount));
          currentState = IDLE;
        }
        break;

      case LEAVE_COMPLETE:
      Serial.println("LEAVE_COMPLETE");
        if (peopleCount > 0) {
          if ((enterDistanceValue > DISTANCE_THRESHOLD) || (enterDistanceValue == -1)) {
            peopleCount--;
            Serial.println("Persona USCITA. Totale: " + String(peopleCount));
            currentState = IDLE;
          }
        } else {
          Serial.println("Errore: Rilevata uscita a stanza già vuota.");
          currentState = IDLE;
        }
        break;
    }
}