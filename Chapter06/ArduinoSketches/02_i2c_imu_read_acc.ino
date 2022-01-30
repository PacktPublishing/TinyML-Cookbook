#include "mbed.h"

#define I2C_SDA p6
#define I2C_SCL p7

#define MPU6050_PWR_MGMT_1       0x6B
#define MPU6050_ADDR_7BIT        0x68
#define MPU6050_ADDR_8BIT        0xD1 // (0x68 << 1)
#define MPU6050_ACCEL_CONFIG     0x1C
#define MPU6050_ACCEL_XOUT_H     0x3B
#define MPU6050_WHO_AM_I         0x75

#define FREQUENCY_HZ             50
#define INTERVAL_MS              (1000 / (FREQUENCY_HZ + 1))
#define INTERVAL_US              INTERVAL_MS * 1000

mbed::I2C i2c(I2C_SDA, I2C_SCL);

void write_reg(int addr_i2c, int addr_reg, char v) {
  char data[2] = {addr_reg, v};
  i2c.write(addr_i2c, data, 2);
  return;
}

void read_reg(int addr_i2c, int addr_reg, char *buf, int length) {
  char data = addr_reg;
  i2c.write(addr_i2c, &data, 1);
  i2c.read(addr_i2c, buf, length);
  return;
}

void read_data(char sad, char sub, char *buf, int length) {
  i2c.write(sad, &sub, 1, true);
  i2c.read(sad, buf, length);
  return;
}

inline void read_accelerometer(float *ax, float *ay, float *az) {
  const float sensitivity = 16384.f;
  const float k = (1.f / sensitivity) * 9.81f;

  char data[6];
  read_reg(MPU6050_ADDR_8BIT, MPU6050_ACCEL_XOUT_H, data, 6);
  int16_t ax_i16 = (int16_t)(data[0] << 8 | data[1]);
  int16_t ay_i16 = (int16_t)(data[2] << 8 | data[3]);
  int16_t az_i16 = (int16_t)(data[4] << 8 | data[5]);
  *ax = (float)ax_i16 * k;
  *ay = (float)ay_i16 * k;
  *az = (float)az_i16 * k;
  return;
}

void setup() {
  i2c.frequency(400000);

  Serial.begin(115600);
  while (!Serial);

  char id;
  read_reg(MPU6050_ADDR_8BIT, MPU6050_WHO_AM_I, &id, 1);

  if (id == MPU6050_ADDR_7BIT) {
    Serial.println("MPU6050 found");
    write_reg(MPU6050_ADDR_8BIT, MPU6050_PWR_MGMT_1, 0x00);
    write_reg(MPU6050_ADDR_8BIT, MPU6050_ACCEL_CONFIG, 0x00);
  } else {
    Serial.println("MPU6050 not found");
    while (1);
  }
}

void loop() {
  mbed::Timer timer;
  timer.start();
  float ax, ay, az;
  read_accelerometer(&ax, &ay, &az);
  Serial.print(ax);
  Serial.print(",");
  Serial.print(ay);
  Serial.print(",");
  Serial.println(az);

  timer.stop();

  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  auto t0 = timer.elapsed_time();
  auto t_diff = duration_cast<microseconds>(t0);
  uint64_t t_wait_us = INTERVAL_US - t_diff.count();
  int32_t t_wait_ms = (t_wait_us / 1000);
  int32_t t_wait_leftover_us = (t_wait_us % 1000);
  delay(t_wait_ms);
  delayMicroseconds(t_wait_leftover_us);
}
