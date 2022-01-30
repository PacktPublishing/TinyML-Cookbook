/* Edge Impulse Arduino examples
 * Copyright (c) 2021 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Includes ---------------------------------------------------------------- */
#include <gestures_recognition_inferencing.h>
#include "mbed.h"

static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static uint32_t run_inference_every_ms = 200;
static rtos::Thread inference_thread(osPriorityLow);
static float buf_sampling[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };
static float buf_inference[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

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

class TestAndTraceFilter {
public:
  static constexpr int32_t invalid_idx_class = -1;

  TestAndTraceFilter(int32_t n, float thr) {
    _thr = thr;
    _n   = n;
  }

  void update(size_t idx_class, float prob) {
    if(idx_class >= _num_classes || prob < _thr) {
      reset();
    }
    else {
      if(prob > _thr) {
        if(idx_class != _last_idx_class) {
          _last_idx_class = idx_class;
          _counter = 0;
        }
        _counter += 1;
      }
      else {
        reset();
      }
    }
  }

  int32_t output() {
    if(_counter > _n) {
      int32_t out = _last_idx_class;
      reset();
      return out;
    }
    else {
      return invalid_idx_class;
    }
  }

private:
  void reset() {
    _counter = 0;
    _last_idx_class = invalid_idx_class;
  }

  // Filtering parameters
  int32_t _n {0};
  float   _thr {0.0f};
  // Utility variables to trace the ML predictions
  int32_t _counter {0};
  int32_t _last_idx_class {invalid_idx_class};
  // Constant to know how many valid classes we can return
  const int32_t _num_classes {3};
};

void inference_func() {
  // wait until we have a full buf_sampling
  delay((EI_CLASSIFIER_INTERVAL_MS * EI_CLASSIFIER_RAW_SAMPLE_COUNT) + 100);

  TestAndTraceFilter filter(4, 0.7f);

  while (1) {
    memcpy(buf_inference, buf_sampling, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(float));

    signal_t signal;
    numpy::signal_from_buffer(buf_inference, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    run_classifier(&signal, &result, debug_nn);

    size_t ix_max = 0;
    float  pb_max = 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if(result.classification[ix].value > pb_max) {
        ix_max = ix;
        pb_max = result.classification[ix].value;
      }
    }

    // Update filtering function
    filter.update(ix_max, pb_max);

    // Get the output
    int32_t out = filter.output();

    if(out != filter.invalid_idx_class) {
      Serial.println(result.classification[out].label);
    }

    delay(run_inference_every_ms);
  }
}

void setup() {
  i2c.frequency(400000);

  Serial.begin(115200);
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

  inference_thread.start(mbed::callback(&inference_func));
}

void loop() {
  mbed::Timer timer;
  timer.start();

  float ax, ay, az;
  read_accelerometer(&ax, &ay, &az);

  numpy::roll(buf_sampling, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, - 3);

  buf_sampling[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3] = ax;
  buf_sampling[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2] = ay;
  buf_sampling[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = az;

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