import os
import pathlib
import re
import sys
from PIL import Image
import numpy as np

# Input quantization parameters
input_quant_offset = -128
input_quant_scale = 0.003921569

def array_to_str(data):
    NUM_COLS = 12
    val_string = ''
    for i, val in enumerate(data):
        val_string += str(val)

        if (i + 1) < len(data):
            val_string += ','
        if (i + 1) % NUM_COLS == 0:
            val_string += '\n'
    return val_string

def gen_c_array(name, size, data):
    str_out = "#include <tvmgen_default.h>\n"
    str_out += f"const unsigned int {name}_len = {size};\n"
    str_out += f'int8_t {name}[] __attribute__((section("ethosu_scratch"), aligned(16))) = '
    str_out += "\n{\n"
    str_out += f'{data}'
    str_out += '\n};'
    return str_out

def gen_input(image_name):

    img_path = os.path.join(f"{image_name}")

    # Resize to 32x32 (Input CIFAR-10)
    resized_image = Image.open(img_path).resize((32, 32))
    img_data = np.asarray(resized_image).astype("float32")

    # Normalize
    img_data /= 255.0

    # Quantize
    img_data /= input_quant_scale
    img_data += input_quant_offset

    # Convert to int8
    input_data = img_data.astype(np.int8)
    input_data = input_data.ravel()

    # Generate a string from array
    val_string = array_to_str(input_data)

    c_code = gen_c_array("input", input_data.size, val_string)

    # Save to file
    with open("include/inputs.h", 'w') as file:
        file.write(c_code)

def gen_output():
    output_data = np.zeros([10], np.int8)

    # Generate a string from array
    val_string = array_to_str(output_data)

    c_code = gen_c_array("output", output_data.size, val_string)

    # Save to file
    with open("include/outputs.h", 'w') as file:
        file.write(c_code)

def gen_labels():
    val_string = "char* labels[] = "
    val_string += '{"airplane", "automobile", "bird", "cat", "deer", '
    val_string += '"dog", "frog", "horse", "ship", "truck"};'
    # Save to file
    with open("include/labels.h", 'w') as file:
        file.write(val_string)

if __name__ == "__main__":
    gen_input(sys.argv[1])
    gen_output()
    gen_labels()
