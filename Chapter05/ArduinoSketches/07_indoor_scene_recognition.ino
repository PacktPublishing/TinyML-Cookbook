#include "indoor_scene_recognition.h"

#include "mbed.h"
#include <Arduino_OV767X.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>

static const char *label[] = {"bathroom", "kitchen", "unknown"};
static int bytes_per_frame;
static int bytes_per_pixel;
static bool debug_application = false;

static uint8_t data[160 * 120 * 2]; // QQVGA: 160x120 X 2 bytes per pixel (YUV422)

static int w0 = 0;
static int h0 = 0;
static int stride_in_y = 0;
static int w1 = 0;
static int h1 = 0;
static float scale_x = 0.0f;
static float scale_y = 0.0f;

template <typename T>
inline T clamp_0_255(T x) {
  return std::max(std::min(x, static_cast<T>(255)), static_cast<T>(0));
}

inline void ycbcr422_rgb888(int32_t Y, int32_t Cb, int32_t Cr, uint8_t* out) {
  Cr = Cr - 128;
  Cb = Cb - 128;

  out[0] = clamp_0_255((int)(Y + Cr + (Cr >> 2) + (Cr >> 3) + (Cr >> 5)));
  out[1] = clamp_0_255((int)(Y - ((Cb >> 2) + (Cb >> 4) + (Cb >> 5)) - ((Cr >> 1) + (Cr >> 3) + (Cr >> 4)) + (Cr >> 5)));
  out[2] = clamp_0_255((int)(Y + Cb + (Cb >> 1) + (Cb >> 2) + (Cb >> 6)));
}

inline uint8_t bilinear_inter(uint8_t v00, uint8_t v01, uint8_t v10, uint8_t v11, float xi_f, float yi_f, int xi, int yi) {
    const float a  = (xi_f - xi);
    const float b  = (1.f - a);
    const float a1 = (yi_f - yi);
    const float b1 = (1.f - a1);

    // Calculate the output
    return clamp_0_255((v00 * b * b1) + (v01 * a * b1) + (v10 * b * a1) + (v11 * a * a1));
}

inline float rescale(float x, float scale, float offset) {
  return (x * scale) - offset;
}

inline int8_t quantize(float x, float scale, float zero_point) {
  return (x / scale) + zero_point;
}

// TensorFlow Lite for Microcontroller global variables
static const tflite::Model* tflu_model            = nullptr;
static tflite::MicroInterpreter* tflu_interpreter = nullptr;
static TfLiteTensor* tflu_i_tensor                = nullptr;
static TfLiteTensor* tflu_o_tensor                = nullptr;
static tflite::MicroErrorReporter tflu_error;

static constexpr int tensor_arena_size = 144000;
static uint8_t *tensor_arena = nullptr;
static float   tflu_scale     = 0.0f;
static int32_t tflu_zeropoint = 0;

void tflu_initialization() {
  Serial.println("TFLu initialization - start");

  tensor_arena = (uint8_t *)malloc(tensor_arena_size);

  // Load the TFLITE model
  tflu_model = tflite::GetModel(indoor_scene_recognition);
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

  // Get the quantization parameters (per-tensor quantization)
  tflu_scale     = i_quantization->scale->data[0];
  tflu_zeropoint = i_quantization->zero_point->data[0];

  Serial.println("TFLu initialization - completed");
}

void setup() {
  Serial.begin(115600);
  while (!Serial);

  if (!Camera.begin(QQVGA, YUV422, 1)) {
    Serial.println("Failed to initialize camera!");
    while (1);
  }

  bytes_per_pixel = Camera.bytesPerPixel();
  bytes_per_frame = Camera.width() * Camera.height() * bytes_per_pixel;

  // Initialize TFLu
  tflu_initialization();

  // Initialize resolution
  w0 = Camera.height();
  h0 = Camera.height();
  stride_in_y = Camera.width() * bytes_per_pixel;
  w1 = 48;
  h1 = 48;

  // Initialize scaling factors
  scale_x = (float)w0 / (float)w1;
  scale_y = (float)h0 / (float)h1;
}

void loop() {
  Camera.readFrame(data);
  uint8_t rgb888[3];
  if(debug_application) {
    Serial.println("<image>");
    Serial.println(w1);
    Serial.println(h1);
  }

  int idx = 0;
  for (int yo = 0; yo < h1; yo++) {
    const float yi_f = (yo * scale_y);
    const int yi = (int)std::floor(yi_f);
    for(int xo = 0; xo < w1; xo++) {
      const float xi_f = (xo * scale_x);
      const int xi = (int)std::floor(xi_f);

      int x0 = xi;
      int y0 = yi;
      int x1 = std::min(xi + 1, w0 - 1);
      int y1 = std::min(yi + 1, h0 - 1);

      // Calculate the offset to access the Y component
      int ix_y00 = x0 * sizeof(int16_t) + y0 * stride_in_y;
      int ix_y01 = x1 * sizeof(int16_t) + y0 * stride_in_y;
      int ix_y10 = x0 * sizeof(int16_t) + y1 * stride_in_y;
      int ix_y11 = x1 * sizeof(int16_t) + y1 * stride_in_y;

      const int Y00 = data[ix_y00];
      const int Y01 = data[ix_y01];
      const int Y10 = data[ix_y10];
      const int Y11 = data[ix_y11];

      // Calculate the offset to access the Cr component
      const int offset_cr00 = xi % 2 == 0? 1 : -1;
      const int offset_cr01 = (xi + 1) % 2 == 0? 1 : -1;

      const int Cr00 = data[ix_y00 + offset_cr00];
      const int Cr01 = data[ix_y01 + offset_cr01];
      const int Cr10 = data[ix_y10 + offset_cr00];
      const int Cr11 = data[ix_y11 + offset_cr01];

      // Calculate the offset to access the Cb component
      const int offset_cb00 = offset_cr00 + 2;
      const int offset_cb01 = offset_cr01 + 2;

      const int Cb00 = data[ix_y00 + offset_cb00];
      const int Cb01 = data[ix_y01 + offset_cb01];
      const int Cb10 = data[ix_y10 + offset_cb00];
      const int Cb11 = data[ix_y11 + offset_cb01];

      uint8_t rgb00[3];
      uint8_t rgb01[3];
      uint8_t rgb10[3];
      uint8_t rgb11[3];

      // Convert YCbCr422 to RGB888
      ycbcr422_rgb888(Y00, Cb00, Cr00, rgb00);
      ycbcr422_rgb888(Y01, Cb01, Cr01, rgb01);
      ycbcr422_rgb888(Y10, Cb10, Cr10, rgb10);
      ycbcr422_rgb888(Y11, Cb11, Cr11, rgb11);

      // Iterate over the RGB channels
      uint8_t c_i;
      float c_f;
      int8_t c_q;
      for(int i = 0; i < 3; i++) {
        c_i = bilinear_inter(rgb00[i], rgb01[i], rgb10[i], rgb11[i], xi_f, yi_f, xi, yi);
        c_f = rescale((float)c_i, 1.f/255.f, -1.f);
        c_q = quantize(c_f, tflu_scale, tflu_zeropoint);
        tflu_i_tensor->data.int8[idx++] = c_q;
        if(debug_application) {
          Serial.println(c_i);
        }
      }
    }
  }
  if(debug_application) {
    Serial.println("</image>");
  }
  // Run inference
  TfLiteStatus invoke_status = tflu_interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("Error invoking the TFLu interpreter");
    return;
  }

  size_t ix_max = 0;
  float  pb_max = 0;
  for (size_t ix = 0; ix < 3; ix++) {
    if(tflu_o_tensor->data.f[ix] > pb_max) {
      ix_max = ix;
      pb_max = tflu_o_tensor->data.f[ix];
    }
  }

  Serial.println(label[ix_max]);
}
