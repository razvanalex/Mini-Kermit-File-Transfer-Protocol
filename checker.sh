#!/bin/bash

make clean && make

rm fisier1.bin recv_fisier1.bin
rm fisier2.bin recv_fisier2.bin
rm fisier3.bin recv_fisier3.bin
rm fisier4.bin recv_fisier4.bin
rm fisier5.bin recv_fisier5.bin
rm fisier6.bin recv_fisier6.bin
rm fisier7.bin recv_fisier7.bin
rm fisier8.bin recv_fisier8.bin
rm fisier9.bin recv_fisier9.bin
rm fisier10.bin recv_fisier10.bin

dd if=/dev/urandom of=fisier1.bin bs=1K count=1
dd if=/dev/urandom of=fisier2.bin bs=2K count=2
dd if=/dev/urandom of=fisier3.bin bs=4K count=2
dd if=/dev/urandom of=fisier4.bin bs=2K count=1
dd if=/dev/urandom of=fisier5.bin bs=1K count=3
dd if=/dev/urandom of=fisier6.bin bs=1K count=1
dd if=/dev/urandom of=fisier7.bin bs=5K count=2
dd if=/dev/urandom of=fisier8.bin bs=20K count=1
dd if=/dev/urandom of=fisier9.bin bs=32K count=1
dd if=/dev/urandom of=fisier10.bin bs=10K count=5

sleep 1
./run_experiment_mod.sh fisier1.bin fisier2.bin fisier3.bin #fisier4.bin fisier5.bin fisier6.bin fisier7.bin fisier8.bin fisier9.bin fisier10.bin
sleep 1

ls -l fisier1.bin recv_fisier1.bin
ls -l fisier2.bin recv_fisier2.bin
ls -l fisier3.bin recv_fisier3.bin
ls -l fisier4.bin recv_fisier4.bin
ls -l fisier5.bin recv_fisier5.bin
ls -l fisier6.bin recv_fisier6.bin
ls -l fisier7.bin recv_fisier7.bin
ls -l fisier8.bin recv_fisier8.bin
ls -l fisier9.bin recv_fisier9.bin
ls -l fisier10.bin recv_fisier10.bin

# diff <(xxd fisier1.bin) <(xxd recv_fisier1.bin)

rm fisier1.bin recv_fisier1.bin
rm fisier2.bin recv_fisier2.bin
rm fisier3.bin recv_fisier3.bin
rm fisier4.bin recv_fisier4.bin
rm fisier5.bin recv_fisier5.bin
rm fisier6.bin recv_fisier6.bin
rm fisier7.bin recv_fisier7.bin
rm fisier8.bin recv_fisier8.bin
rm fisier9.bin recv_fisier9.bin
rm fisier10.bin recv_fisier10.bin
