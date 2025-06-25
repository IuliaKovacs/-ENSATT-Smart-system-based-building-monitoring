#ifndef LED_ARRAY_H
#define LED_ARRAY_H

#include <Adafruit_NeoPixel.h>

#define RGB_LED_STICK_PIN 6
#define RGB_NO_PIXELS 10

void initLedArray();
void redBlinkingLight();
void orangeSlowlyBlinkingLight();
void playGreenWaveEntryEffect();
void playGreenWaveLeavingEffect();
void turnOffAllPixels();

#endif