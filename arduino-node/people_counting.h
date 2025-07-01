#ifndef PEOPLE_COUNTING_H
#define PEOPLE_COUNTING_H

#include <Arduino.h>
#include <HCSR04.h>
#include <Wire.h>

#define ENTER_TRIG_PIN 10
#define ENTER_ECHO_PIN 11

#define LEAVE_TRIG_PIN 12
#define LEAVE_ECHO_PIN 13

#define DISTANCE_THRESHOLD 10

#define IR_CAMERA_ADDR 0x33
#define HUMAN_DETECTION_THRESHOLD -110.0

// Defining states for the finite state machine
enum State {
  IDLE,
  ENTER_DETECT,
  LEAVE_DETECT,
  ENTER_COMPLETE,
  LEAVE_COMPLETE
};

void initInfraredCamera();
void initUltrasonicSensors();
void counterSysSTM();
void readMatrix();


#endif