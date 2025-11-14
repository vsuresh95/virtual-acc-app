#!/bin/sh

MODELS="16 32 48 64"
ITERS="4000 2000 1000 500"

# helper: MODELS[size]
get_model() {
    idx=$1
    set -- $MODELS
    shift "$idx"
    echo "$1"
}

# helper: ITERS[size]
get_iter() {
    idx=$1
    set -- $ITERS
    shift "$idx"
    echo "$1"
}

APP_DIR=./opt.exe

run_model() {
    model_size=$1
    threads=$2
    chains=$3
    iterations=$4

    echo "Model: $model_size, Threads: $threads, Chains: $chains, Iterations: $iterations"
    "$APP_DIR" "$iterations" "$threads" "models/model_${model_size}_${chains}.txt"
}

mv /dev/gemm_stratus.1 ./gemm_stratus.1
mv /dev/gemm_stratus.2 ./gemm_stratus.2
mv /dev/gemm_stratus.3 ./gemm_stratus.3
mv /dev/gemm_stratus.4 ./gemm_stratus.4
mv /dev/gemm_stratus.5 ./gemm_stratus.5

for size in 0 1 2 3; do
    for chains in 1; do
        dev_index=$((chains - 1))
        for threads in 1 2 3 4; do
            run_model "$(get_model "$size")" "$threads" "$chains" "$(get_iter "$size")"
        done
    done
done