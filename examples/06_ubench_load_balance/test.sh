#!/bin/sh

mv /dev/gemm_sm_stratus.2 ./gemm_sm_stratus.2
mv /dev/gemm_sm_stratus.3 ./gemm_sm_stratus.3
mv /dev/gemm_sm_stratus.4 ./gemm_sm_stratus.4
mv /dev/gemm_sm_stratus.5 ./gemm_sm_stratus.5

j=16
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe 20000 models/model_16_1.txt
done

j=32
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe 10000 models/model_32_1.txt
done

j=48
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe 5000 models/model_48_1.txt
done

j=64
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe 3000 models/model_64_1.txt
done

./default_opt.exe 10000 models/model_16_1.txt
./default_opt.exe 10000 models/model_32_1.txt
./default_opt.exe 5000 models/model_48_1.txt
./default_opt.exe 3000 models/model_64_1.txt