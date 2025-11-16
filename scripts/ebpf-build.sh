#!/bin/bash
set -e

bpftool btf dump file /sys/kernel/btf/vmlinux format c > ./loader/vmlinux.h
clang -g -O2 -target bpf -c ./loader/monitor.c -o ./loader/monitor.o
bpftool gen skeleton ./loader/monitor.o > ./loader/monitor.skel.h
clang ./loader/loader.c -o ebpf-loader -lbpf -lelf

rm ./loader/vmlinux.h
rm ./loader/monitor.o
rm ./loader/monitor.skel.h
