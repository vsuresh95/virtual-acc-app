#!/bin/sh
echo "CHAIN 64"
i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 1000 model_64_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 1000 model_64_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 1000 model_64_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 1000 model_64_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./chain_opt.exe 1000 model_64_5.txt
  i=$((i + 1))
done

echo "PIPE 64"

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 1000 model_64_1.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 1000 model_64_2.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 1000 model_64_3.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 1000 model_64_4.txt
  i=$((i + 1))
done

i=1
while [ "$i" -le 5 ]; do
  ./pipe_opt.exe 1000 model_64_5.txt
  i=$((i + 1))
done
