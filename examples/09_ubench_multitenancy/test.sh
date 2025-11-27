#!/bin/sh

MODELS="16 32 48 64"
ITERS="20000 10000 5000 2500"
MOZART_APP=./mozart_opt.exe
VIRTUOSO_APP=./opt.exe

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

run_model() {
    model_size=$1
    threads=$2
    chains=$3
    iterations=$4
    app=$5

    echo "Model: $model_size, Threads: $threads, Chains: $chains, Iterations: $iterations"
    "$app" "$iterations" "$threads" "models/model_${model_size}_${chains}.txt"
}

mv /dev/gemm_sm_stratus.0 ./gemm_sm_tratus.0
mv /dev/gemm_sm_stratus.1 ./gemm_sm_tratus.1
mv /dev/gemm_sm_stratus.2 ./gemm_sm_tratus.2
mv /dev/gemm_sm_stratus.3 ./gemm_sm_tratus.3
mv /dev/gemm_sm_stratus.4 ./gemm_sm_tratus.4
mv /dev/gemm_sm_stratus.5 ./gemm_sm_tratus.5

for threads in 1 2 3 4 5 6; do
    dev_index=$((threads - 1))
    mv ./gemm_sm_tratus.$dev_index /dev/gemm_sm_stratus.$dev_index 
    for chains in 1 2 3 4; do
        for size in 0 1 2 3; do
            iters=$(( $(get_iter "$size") / chains ))
            model=$(get_model "$size")
            run_model "$model" "$threads" "$chains" "$iters" "$VIRTUOSO_APP"
        done
    done
done

mv /dev/gemm_sm_stratus.0 ./gemm_sm_tratus.0
mv /dev/gemm_sm_stratus.1 ./gemm_sm_tratus.1
mv /dev/gemm_sm_stratus.2 ./gemm_sm_tratus.2
mv /dev/gemm_sm_stratus.3 ./gemm_sm_tratus.3
mv /dev/gemm_sm_stratus.4 ./gemm_sm_tratus.4
mv /dev/gemm_sm_stratus.5 ./gemm_sm_tratus.5

for threads in 1 2 3 4 5 6; do
    dev_index=$((threads - 1))
    mv ./gemm_sm_tratus.$dev_index /dev/gemm_sm_stratus.$dev_index 
    for chains in 1 2 3 4; do
        for size in 0 1 2 3; do
            iters=$(( $(get_iter "$size") / chains ))
            model=$(get_model "$size")
            run_model "$model" "$threads" "$chains" "$iters" "$MOZART_APP"
        done
    done
done
