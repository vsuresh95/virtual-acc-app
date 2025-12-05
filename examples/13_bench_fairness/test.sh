#!/bin/sh

mv /dev/gemm_sm_stratus.4 ./gemm_sm_stratus.4
mv /dev/gemm_sm_stratus.5 ./gemm_sm_stratus.5
 
./opt.exe 0 4 0
./opt.exe 1 4 0
./opt.exe 2 4 0

mv /dev/gemm_sm_stratus.1 ./gemm_sm_stratus.1
mv /dev/gemm_sm_stratus.2 ./gemm_sm_stratus.2
mv /dev/gemm_sm_stratus.3 ./gemm_sm_stratus.3

./opt.exe 0 1 0
./opt.exe 0 1 1
./opt.exe 0 1 2
./opt.exe 0 1 3
./opt.exe 1 1 0
./opt.exe 1 1 1
./opt.exe 1 1 2
./opt.exe 1 1 3
./opt.exe 2 1 0
./opt.exe 2 1 1
./opt.exe 2 1 2
./opt.exe 2 1 3