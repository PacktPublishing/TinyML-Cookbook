#include "mbed.h"

#if defined(ARDUINO_ARDUINO_NANO33BLE)
static const PinName gpio_pin_out = p23;
#endif

#if defined(ARDUINO_RASPBERRY_PI_PICO)
static const PinName gpio_pin_out = p22;
#endif

static mbed::DigitalOut led(gpio_pin_out);

void setup() {
}

void loop() {
  led = 1;
}
