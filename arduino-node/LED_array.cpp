#include "LED_array.h"

#define COLOR_RED    150, 0, 0
#define COLOR_GREEN  0, 150, 0
#define COLOR_ORANGE 255, 80, 0

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(RGB_NO_PIXELS, RGB_LED_STICK_PIN, NEO_GRB + NEO_KHZ800);

int delayval = 100;
int blinkDelay = 300;


void initLedArray() {
  pixels.setBrightness(255);
  pixels.begin();
}

void redBlinkingLight() {
  for (int counter = 0; counter < 10; counter++) {
    for (int i = RGB_NO_PIXELS - 1; i >= 0; i--) {
      pixels.setPixelColor(i, pixels.Color(COLOR_RED));
      pixels.show();
    }
    delay(blinkDelay);
    turnOffAllPixels();
    delay(blinkDelay);
  }
}

void orangeSlowlyBlinkingLight() {
  unsigned long t = millis();
  float phase = (t % 2000) / 2000.0 * TWO_PI;
  float intensity = (sin(phase) + 1.0) / 2.0;

  int red = (intensity * 255);
  int green = (intensity * 80);
  int blue = 0;

  for (int i = 0; i < RGB_NO_PIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
  }

  pixels.show();
}

void playGreenWaveEntryEffect() {
  float frequency = PI / 5.0;
  float waveOffset = 0.0;

  while (waveOffset < RGB_NO_PIXELS) {
    for (int i = 0; i < RGB_NO_PIXELS; i++) {
      float x = i - waveOffset;
      float value = sin(frequency * x);

      int brightness = 0;
      if (x >= -PI && x <= PI) {
        brightness = (value + 1.0) * 127.5;
      }

      pixels.setPixelColor(i, pixels.Color(0, brightness, 0));
    }

    pixels.show();
    waveOffset += 0.5;
    delay(30);
  }

  delay(200);
  turnOffAllPixels();
}

void playGreenWaveLeavingEffect() {
  float frequency = PI / 5.0;
  float waveOffset = RGB_NO_PIXELS;

  while (waveOffset > 0.0) {
    for (int i = 0; i < RGB_NO_PIXELS; i++) {
      float x = i - waveOffset;
      float value = sin(frequency * x);

      int brightness = 0;
      if (x >= -PI && x <= PI) {
        brightness = (value + 1.0) * 127.5;
      }

      pixels.setPixelColor(i, pixels.Color(0, brightness, 0));
    }

    pixels.show();
    waveOffset -= 0.5;
    delay(30);
  }

  delay(200);
  turnOffAllPixels();
}

void turnOffAllPixels() {
  for (int i = 0; i < RGB_NO_PIXELS; i++) {
    pixels.setPixelColor(i, 0);
  }
  pixels.show();
}