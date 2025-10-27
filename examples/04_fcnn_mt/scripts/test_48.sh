#!/bin/sh
echo "CHAIN 48"
i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 2000 model_48_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 2000 model_48_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 2000 model_48_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 2000 model_48_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 2000 model_48_5.txt
  i=$((i + 1))
done

echo "PIPE 48"

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 2000 model_48_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 2000 model_48_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 2000 model_48_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 2000 model_48_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 2000 model_48_5.txt
  i=$((i + 1))
done
