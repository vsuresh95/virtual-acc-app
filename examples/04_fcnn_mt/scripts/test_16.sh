#!/bin/sh
echo "CHAIN 16"
i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 10000 model_16_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 10000 model_16_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 10000 model_16_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 10000 model_16_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 10000 model_16_5.txt
  i=$((i + 1))
done

echo "PIPE 16"

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 10000 model_16_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 10000 model_16_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 10000 model_16_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 10000 model_16_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 10000 model_16_5.txt
  i=$((i + 1))
done
