/*
 * Copyright 2020 The TensorFlow Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "main_functions.h"

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include "model.h"
#include "input.h"
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/system_setup.h>
#include <tensorflow/lite/schema/schema_generated.h>

namespace {
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;

    constexpr int tensor_arena_size = 52000;
    uint8_t *tensor_arena = nullptr;
    tflite::MicroMutableOpResolver<5> resolver;

    static const char *label[] ={"airplane", "automobile", "bird", "cat", "deer",
                                 "dog", "frog", "horse", "ship", "truck"};
    float   o_scale      = 0.0f;
    int32_t o_zero_point = 0;
}

void setup(void) {
    static tflite::MicroErrorReporter micro_error_reporter;

    error_reporter = &micro_error_reporter;

    tensor_arena = (uint8_t *)malloc(tensor_arena_size);

    model = tflite::GetModel(cifar10_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Error loading the TFLite model\n");
        while(1);
    }

    resolver.AddFullyConnected();
    resolver.AddDepthwiseConv2D();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, tensor_arena_size, error_reporter);

    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        printf("Error allocating the tensors\n");
        while(1);
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    const auto* o_quantization = reinterpret_cast<TfLiteAffineQuantization*>(output->quantization.params);
    o_scale      = o_quantization->scale->data[0];
    o_zero_point = o_quantization->zero_point->data[0];
}


void loop(void) {
    for(int i = 0; i < g_test_len; i++) {
        input->data.int8[i] = g_test[i];
    }

    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        printf("Error running model inference\n");
        while(1);
    }

    size_t ix_max = 0;
    float  pb_max = 0;
    for (size_t ix = 0; ix < 10; ix++) {
        int8_t out_val = output->data.int8[ix];
        float  pb = ((float)out_val - o_zero_point) * o_scale;
        if(pb > pb_max) {
            ix_max = ix;
            pb_max = pb;
        }
    }

    if(ix_max == g_test_label) {
        printf("CORRECT classification! %s\n", label[ix_max]);
        while(1);
    }
    else {
        printf("WRONG classification! %s\n", label[ix_max]);
    }
}
