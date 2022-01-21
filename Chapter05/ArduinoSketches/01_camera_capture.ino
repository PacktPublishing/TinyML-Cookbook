#include "mbed.h"
#include <Arduino_OV767X.h>

#define PRESSED     0
#define BUTTON      p30

static mbed::DigitalIn  button(BUTTON);

int bytes_per_frame;

byte data[320 * 240 * 2];

void setup() {
  Serial.begin(115600);
  while (!Serial);

  Serial.println("OV767X Camera Capture");
  Serial.println();

  if (!Camera.begin(QVGA, RGB565, 1)) {
    Serial.println("Failed to initialize camera!");
    while (1);
  }

  button.mode(PullUp);

  bytes_per_frame = Camera.width() * Camera.height() * Camera.bytesPerPixel();
}

void loop() {
  if(button == PRESSED) {
    Serial.println("Reading frame");
    Serial.println();

    Camera.readFrame(data);
    Serial.write(data, bytes_per_frame);
  }
}