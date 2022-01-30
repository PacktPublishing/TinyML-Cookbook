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
#include "mbed.h"
#include "hardware/adc.h"

// If your target is limited in memory remove this macro to save 10K RAM
#define EIDSP_QUANTIZE_FILTERBANK   0

/* Includes ---------------------------------------------------------------- */
#include <voice_controlling_led_inferencing.h>

#define ON          1
#define OFF         0
#define PRESSED     0
#define LEDR        p9
#define LEDG        p8
#define LEDB        p7
#define LED_BUILTIN p25
#define BUTTON      p10
#define BIAS_MIC    1552 // (1.25V * 4095) / 3.3
#define AUDIO_SAMPLING_RATE 16000.0
#define NUM_COLORS  3
#define NUM_NUMBERS 3
#define PROBABILITY_THR 0.5
#define GAIN        1

static mbed::Ticker     timer;
static mbed::DigitalOut rgb[] = {LEDR, LEDG, LEDB};
static mbed::DigitalOut led_builtin(LED_BUILTIN);
static mbed::DigitalIn  button(BUTTON);

/** Audio buffers, pointers and selectors */
typedef struct {
    int16_t *buffer;
    int16_t *buffer_filtered;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static volatile inference_t inference;
static bool debug_nn = false;
static bool debug_raw_audio = true;

static volatile int  ix_buffer       = 0;
static volatile bool is_buffer_ready = false;

static size_t current_color = 0;

static size_t map_encoded_labels(size_t ix_label) {
  static size_t map_array[EI_CLASSIFIER_LABEL_COUNT] = { 2, 1, 3, 0, 5, 4, 6 };
  if(ix_label >= EI_CLASSIFIER_LABEL_COUNT) {
    Serial.print("Out of bound range\n");
    while(1);
  }
  return map_array[ix_label];
}

static bool is_color(size_t ix) {
  if(ix < NUM_COLORS) {
    return true;
  }
  else {
    return false;
  }
}

static bool is_number(size_t ix) {
  if(ix >= NUM_COLORS && ix < (EI_CLASSIFIER_LABEL_COUNT - 1)) {
    return true;
  }
  else {
    return false;
  }
}

static void adc_setup() {
  adc_init();
  adc_gpio_init(26);
  adc_select_input(0);
}

static void print_raw_audio() {
  for(int i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; ++i) {
    ei_printf("%d\n", inference.buffer[i]);
  }
}

void timer_ISR() {
  if(ix_buffer < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
    int16_t v = (int16_t)((adc_read() - BIAS_MIC)) * GAIN;
    inference.buffer[ix_buffer++] = (int16_t)v;
  }
  else {
    is_buffer_ready = true;
  }
}

/**
 * @brief      Arduino setup function
 */
void setup()
{
  Serial.begin(115200);

  while(!Serial);

  adc_setup();

  led_builtin = 0;

  rgb[0] = OFF;
  rgb[1] = OFF;
  rgb[2] = OFF;
  rgb[current_color] = ON;
  button.mode(PullUp);

  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
      ei_printf("ERR: Failed to setup audio sampling\r\n");
      return;
  }
}

/**
 * @brief      Arduino main function. Runs the inferencing loop.
 */
void loop()
{
  if(button == PRESSED) {
    delay(700);

    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }

    // Get the index with higher probability
    size_t ix_max = 0;
    float  pb_max = 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if(result.classification[ix].value > pb_max) {
        ix_max = ix;
        pb_max = result.classification[ix].value;
      }
    }

    // Map the encoded label to our representation (red, green, blue, one, two, three)
    const size_t ix_max0 = map_encoded_labels(ix_max);

    if(pb_max > PROBABILITY_THR) {
      if(is_color(ix_max0)) {
        // Convert the index to LED pin number
        const size_t new_color = ix_max0;

        if(new_color != current_color) {
          rgb[current_color] = OFF; // Turn off current_color
          rgb[new_color] = ON;     // Turn on new_color
          current_color = new_color;
        }
      }

      if(is_number(ix_max0)) {
        const size_t num_blinks = ix_max0 - NUM_COLORS + 1;
        for(size_t i = 0; i < num_blinks; ++i) {
          rgb[current_color] = OFF;
          delay(1000);
          rgb[current_color] = ON;
          delay(1000);
        }
      }
    }
    while(button == PRESSED);
    delay(1000);
  }
}

/**
 * @brief      Printf function uses vsnprintf and output using Arduino Serial
 *
 * @param[in]  format     Variable argument list
 */
void ei_printf(const char *format, ...) {
  static char print_buf[1024] = { 0 };

  va_list args;
  va_start(args, format);
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
  va_end(args);

  if (r > 0) {
      Serial.write(print_buf);
  }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

  if(inference.buffer == NULL) {
      return false;
  }

  inference.buf_count  = 0;
  inference.n_samples  = n_samples;
  inference.buf_ready  = 0;

  return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void) {
  unsigned int sampling_period_us = 1000000 / 16000;

  ix_buffer = 0;
  is_buffer_ready = false;
  led_builtin = ON;
  timer.attach_us(&timer_ISR, sampling_period_us);

  while(!is_buffer_ready);

  timer.detach();
  led_builtin = OFF;

  if(debug_raw_audio) {
    print_raw_audio();
  }

  return true;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

    return 0;
}

static void microphone_inference_end(void) {
  free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
