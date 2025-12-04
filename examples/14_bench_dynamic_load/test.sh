#!/bin/sh

mv /dev/gemm_sm_stratus.4 ./gemm_sm_stratus.4
mv /dev/gemm_sm_stratus.5 ./gemm_sm_stratus.5

# ./mozart_opt.exe 0 4 0
# ./mozart_opt.exe 0 1 0
# ./mozart_opt.exe 0 1 1
# ./mozart_opt.exe 0 1 2
# ./mozart_opt.exe 0 1 3
./opt.exe 0 4 0
./opt.exe 0 1 0
./opt.exe 0 1 1
./opt.exe 0 1 2
./opt.exe 0 1 3

# ./mozart_opt.exe 1 4 0
# ./mozart_opt.exe 1 1 0
# ./mozart_opt.exe 1 1 1
# ./mozart_opt.exe 1 1 2
# ./mozart_opt.exe 1 1 3
./opt.exe 1 4 0
./opt.exe 1 1 0
./opt.exe 1 1 1
./opt.exe 1 1 2
./opt.exe 1 1 3

# ./mozart_opt.exe 2 4 0
# ./mozart_opt.exe 2 1 0
# ./mozart_opt.exe 2 1 1
# ./mozart_opt.exe 2 1 2
# ./mozart_opt.exe 2 1 3
./opt.exe 2 4 0
./opt.exe 2 1 0
./opt.exe 2 1 1
./opt.exe 2 1 2
./opt.exe 2 1 3