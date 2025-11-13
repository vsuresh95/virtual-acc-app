#!/bin/sh

for j in 16 32 48 64; do
    for m in 1 4 16 64 256; do
        ./${j}_${m}.exe ${j} ${j} ${j} 3000
    done
done
