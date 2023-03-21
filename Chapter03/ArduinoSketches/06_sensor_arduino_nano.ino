#include <Arduino_HTS221.h>

#define READ_TEMPERATURE() HTS.readTemperature()
#define READ_HUMIDITY()    HTS.readHumidity()

void setup() {
  Serial.begin(9600);

  while (!Serial);

  if (!HTS.begin()) {

    Serial.println("Failed initialization of HTS221!");
    while (1);
  }
}

void loop() {
  //  Moved the read sensor values stuff from setup() to here
  Serial.print("Test Temperature = ");
  Serial.print(READ_TEMPERATURE(), 2);
  Serial.println(" °C");
  Serial.print("Test Humidity = ");
  Serial.print(READ_HUMIDITY(), 2);
  Serial.println(" %");

  delay(2000);
}
