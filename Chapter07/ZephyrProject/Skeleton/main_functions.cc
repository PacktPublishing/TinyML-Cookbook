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

    constexpr int kTensorArenaSize = 2000;
    uint8_t tensor_arena[kTensorArenaSize];
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

    /* This pulls in all the operation implementations we need.
     * NOLINTNEXTLINE(runtime-global-variables)
     */
    static tflite::AllOpsResolver resolver;

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
}


void loop(void) {
}
