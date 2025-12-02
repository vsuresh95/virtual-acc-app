#!/bin/sh
set -eu

OUTDIR=/scratch/vv15/jan_25_audio_virtual_exp/socs/xilinx-vcu118-xcvu9p-backup/soft-build/ariane/sysroot/applications/test/06_ubench_load_balance

j=16
i=20000
for m in 1 2 4 8 16; do
    make clean
    SLEEP_TIME=$((m * i)) make
    cp "$OUTDIR/opt.exe" "$OUTDIR/${j}_${m}_opt.exe"
done

j=32
i=90000
for m in 1 2 4 8 16; do
    make clean
    SLEEP_TIME=$((m * i)) make
    cp "$OUTDIR/opt.exe" "$OUTDIR/${j}_${m}_opt.exe"
done

j=48
i=300000
for m in 1 2 4 8 16; do
    make clean
    SLEEP_TIME=$((m * i)) make
    cp "$OUTDIR/opt.exe" "$OUTDIR/${j}_${m}_opt.exe"
done

j=64
i=700000
for m in 1 2 4 8 16; do
    make clean
    SLEEP_TIME=$((m * i)) make
    cp "$OUTDIR/opt.exe" "$OUTDIR/${j}_${m}_opt.exe"
done

make clean
SLEEP_TIME=19531250 make
cp "$OUTDIR/opt.exe" "$OUTDIR/default_opt.exe"
