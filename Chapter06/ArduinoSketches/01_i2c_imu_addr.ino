#include "mbed.h"

#define I2C_SDA p6
#define I2C_SCL p7

#define MPU6050_ADDR_7BIT        0x68
#define MPU6050_ADDR_8BIT        0xD1 // (0x68 << 1)
#define MPU6050_WHO_AM_I         0x75

mbed::I2C i2c(I2C_SDA, I2C_SCL);


void read_reg(int addr_i2c, int addr_reg, char *buf, int length) {
  char data = addr_reg;
  i2c.write(addr_i2c, &data, 1);
  i2c.read(addr_i2c, buf, length);
  return;
}

void setup() {
  i2c.frequency(400000);

  Serial.begin(115600);
  while (!Serial);

  char id;
  read_reg(MPU6050_ADDR_8BIT, MPU6050_WHO_AM_I, &id, 1);

  if(id == MPU6050_ADDR_7BIT) {
    Serial.println("MPU6050 found");
  } else {
    Serial.println("MPU6050 not found");
    while(1);
  }
}

void loop() {
}
