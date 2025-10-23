#!/bin/sh
echo "SIZE 16"
i=1
while [ "$i" -le 5 ]; do
  j=1
  while [ "$j" -le 5 ]; do
    ./opt.exe 16 16 16 5000 $i
    j=$((j + 1))
  done
  i=$((i + 1))
done

echo "SIZE 32"
i=1
while [ "$i" -le 5 ]; do
  j=1
  while [ "$j" -le 5 ]; do
    ./opt.exe 32 32 32 3000 $i
    j=$((j + 1))
  done
  i=$((i + 1))
done

echo "SIZE 48"
i=1
while [ "$i" -le 5 ]; do
  j=1
  while [ "$j" -le 5 ]; do
    ./opt.exe 48 48 48 2000 $i
    j=$((j + 1))
  done
  i=$((i + 1))
done

echo "SIZE 64"
i=1
while [ "$i" -le 5 ]; do
  j=1
  while [ "$j" -le 5 ]; do
    ./opt.exe 64 64 64 1000 $i
    j=$((j + 1))
  done
  i=$((i + 1))
done
