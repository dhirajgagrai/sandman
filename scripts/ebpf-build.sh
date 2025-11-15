#!/bin/bash
set -e

clang -g -O2 -target bpf -c ./loader/monitor.c -o ./loader/monitor.o
bpftool gen skeleton ./loader/monitor.o > ./loader/monitor.skel.h
clang ./loader/loader.c -o ebpf-loader -lbpf -lelf

rm ./loader/monitor.o
rm ./loader/monitor.skel.h
