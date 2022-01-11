// Note: Set to 1 if you want to check whether the model can forecast the snow
#define DEBUG_SNOW 0

#include "snow_forecast_model.h"

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>

#define NUM_HOURS 3

constexpr int num_reads = 3;
constexpr float t_mean  = 2.05179f;
constexpr float h_mean  = 82.30551f;
constexpr float t_std   = 7.33084f;
constexpr float h_std   = 14.55707f;

// Circular buffers for the input features
int8_t t_vals[NUM_HOURS] = {0};
int8_t h_vals[NUM_HOURS] = {0};
int cur_idx                = 0;
bool is_valid              = false;

// TensorFlow Lite for Microcontroller global variables
const tflite::Model* tflu_model            = nullptr;
tflite::MicroInterpreter* tflu_interpreter = nullptr;
TfLiteTensor* tflu_i_tensor                = nullptr;
TfLiteTensor* tflu_o_tensor                = nullptr;
tflite::MicroErrorReporter tflu_error;

constexpr int tensor_arena_size = 4 * 1024;
byte tensor_arena[tensor_arena_size] __attribute__((aligned(16)));
float   tflu_i_scale      = 0.0f;
float   tflu_o_scale      = 0.0f;
int32_t tflu_i_zero_point = 0;
int32_t tflu_o_zero_point = 0;

inline int8_t quantize(float x, float scale, float zero_point)
{
  return (x / scale) + zero_point;
}

inline float dequantize(int8_t x, float scale, float zero_point)
{
  return ((float)x - zero_point) * scale;
}

void tflu_initialization()
{
  Serial.println("TFLu initialization - start");

  // Load the TFLITE model
  tflu_model = tflite::GetModel(snow_forecast_model_tflite);
  if (tflu_model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.print(tflu_model->version());
    Serial.println("");
    Serial.print(TFLITE_SCHEMA_VERSION);
    Serial.println("");
    while(1);
  }

  tflite::AllOpsResolver tflu_ops_resolver;

  // Initialize the TFLu interpreter
  tflu_interpreter = new tflite::MicroInterpreter(tflu_model, tflu_ops_resolver, tensor_arena, tensor_arena_size, &tflu_error);

  // Allocate TFLu internal memory
  tflu_interpreter->AllocateTensors();

  // Get the pointers for the input and output tensors
  tflu_i_tensor = tflu_interpreter->input(0);
  tflu_o_tensor = tflu_interpreter->output(0);

  const auto* i_quantization = reinterpret_cast<TfLiteAffineQuantization*>(tflu_i_tensor->quantization.params);
  const auto* o_quantization = reinterpret_cast<TfLiteAffineQuantization*>(tflu_o_tensor->quantization.params);

  // Get the quantization parameters (per-tensor quantization)
  tflu_i_scale      = i_quantization->scale->data[0];
  tflu_i_zero_point = i_quantization->zero_point->data[0];
  tflu_o_scale      = o_quantization->scale->data[0];
  tflu_o_zero_point = o_quantization->zero_point->data[0];

  Serial.println("TFLu initialization - completed");
}

#if defined(ARDUINO_ARDUINO_NANO33BLE)
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

  tflu_initialization();
}
#endif // defined(ARDUINO_NANO33_BLE_SENSE)

#if defined(ARDUINO_RASPBERRY_PI_PICO)
#include <DHT.h>

const int gpio_pin_dht_pin = 10;

DHT dht(gpio_pin_dht_pin, DHT11);

#define READ_TEMPERATURE() dht.readTemperature()
#define READ_HUMIDITY()    dht.readHumidity()

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Initialize the DHT sensor
  dht.begin();

  // Waiting for the peripheral for being ready
  delay(2000);

  tflu_initialization();
}
#endif // defined(RASPBERRY_PI_PICO)

void loop() {
  float t = 0.0f;
  float h = 0.0f;

#if DEBUG_SNOW == 1
  t = -10.0f;
  h = 100.0f;
#else
  for(int i = 0; i < num_reads; ++i) {
    t += READ_TEMPERATURE();
    h += READ_HUMIDITY();

    delay(3000);
  }

  // Take the average
  t /= (float)num_reads;
  h /= (float)num_reads;
#endif

  Serial.print("Temperature = ");
  Serial.print(t, 2);
  Serial.println(" °C");
  Serial.print("Humidity = ");
  Serial.print(h, 2);
  Serial.println(" %");

  // Z-score scaling
  t = (t - t_mean) / t_std;
  h = (h - h_mean) / h_std;

  // Store the normalized and quantized samples in the circular buffers
  t_vals[cur_idx] = quantize(t, tflu_i_scale, tflu_i_zero_point);
  h_vals[cur_idx] = quantize(h, tflu_i_scale, tflu_i_zero_point);

  // Get index for the last three samples
  const int idx0 = cur_idx;
  const int idx1 = (cur_idx - 1 + NUM_HOURS) % NUM_HOURS;
  const int idx2 = (cur_idx - 2 + NUM_HOURS) % NUM_HOURS;

  // Initialize the input tensor
  tflu_i_tensor->data.int8[0] = t_vals[idx2];
  tflu_i_tensor->data.int8[1] = t_vals[idx1];
  tflu_i_tensor->data.int8[2] = t_vals[idx0];
  tflu_i_tensor->data.int8[3] = h_vals[idx2];
  tflu_i_tensor->data.int8[4] = h_vals[idx1];
  tflu_i_tensor->data.int8[5] = h_vals[idx0];

  // Run inference
  TfLiteStatus invoke_status = tflu_interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("Error invoking the TFLu interpreter");
    return;
  }

  int8_t out_int8 = tflu_o_tensor->data.int8[0];
  float out_f = dequantize(out_int8, tflu_o_scale, tflu_o_zero_point);

  // After the first three samples, is_valid will be always true
  is_valid = is_valid || cur_idx == 2;

  if (is_valid) {
    if(out_f > 0.5) {
      Serial.println("Yes, it snows");
    }
    else {
      Serial.println("No, it does not snow");
    }
  }

  Serial.println();

  // Update the circular buffer index
  cur_idx = (cur_idx + 1) % NUM_HOURS;

  // We should have a delay of 1 hour but, for practical reasons, we have reduced it to 2 seconds
  delay(2000);
}