#include "mbed.h"
#include <Arduino_OV767X.h>

#define PRESSED     0
#define BUTTON      p30

static mbed::DigitalIn  button(BUTTON);

int bytes_per_frame;
int bytes_per_pixel;

uint8_t data[320 * 240 * 2];

inline void rgb565_rgb888(uint8_t* in, uint8_t* out) {
  uint16_t p = (in[0] << 8) | in[1];

  out[0] = ((p >> 11) & 0x1f) << 3;
  out[1] = ((p >> 5) & 0x3f) << 2;
  out[2] = (p & 0x1f) << 3;
}

void setup() {
  Serial.begin(115600);
  while (!Serial);

  if (!Camera.begin(QVGA, RGB565, 1)) {
    Serial.println("Failed to initialize camera!");
    while (1);
  }

  bytes_per_pixel = Camera.bytesPerPixel();
  bytes_per_frame = Camera.width() * Camera.height() * bytes_per_pixel;

  Camera.testPattern();
}

void loop() {
  if(button == PRESSED) {
    Camera.readFrame(data);
    uint8_t rgb888[3];
    Serial.println("<image>");
    Serial.println(Camera.width());
    Serial.println(Camera.height());
    for(int i = 0; i < bytes_per_frame; i+=bytes_per_pixel) {
      rgb565_rgb888(&data[i], &rgb888[0]);
      Serial.println(rgb888[0]);
      Serial.println(rgb888[1]);
      Serial.println(rgb888[2]);
    }
    Serial.println("</image>");
  }
}
