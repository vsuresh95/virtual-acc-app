#!/bin/sh
echo "CHAIN 32"
i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 5000 model_32_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 5000 model_32_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 5000 model_32_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 5000 model_32_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 5000 model_32_5.txt
  i=$((i + 1))
done

echo "PIPE 32"

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 5000 model_32_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 5000 model_32_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 5000 model_32_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 5000 model_32_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 5000 model_32_5.txt
  i=$((i + 1))
done
