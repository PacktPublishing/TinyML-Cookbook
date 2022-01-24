# Compile the model
python3.6 -m tvm.driver.tvmc compile --target="ethos-u -accelerator_config=ethos-u55-256, c" \
    --target-c-mcpu=cortex-m55 \
    --runtime=crt \
    --executor=aot \
    --executor-aot-interface-api=c \
    --executor-aot-unpacked-api=1 \
    --output-format=mlf \
    --pass-config tir.disable_vectorize=1 \
    cifar10_int8.tflite

# Untar the model
tar -C build -xvf module.tar
