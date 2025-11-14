#!/bin/sh

j=16
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe ${j} ${j} ${j} 10000
done

j=32
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe ${j} ${j} ${j} 10000
done

j=48
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe ${j} ${j} ${j} 5000
done

j=64
for m in 1 2 4 8 16; do
    ./${j}_${m}_opt.exe ${j} ${j} ${j} 3000
done

./default_opt.exe 16 16 16 10000
./default_opt.exe 32 32 32 10000
./default_opt.exe 48 48 48 5000
./default_opt.exe 64 64 64 3000
