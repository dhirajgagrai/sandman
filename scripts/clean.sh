#!/bin/bash

rm -rf ./build/*
rm -rf ./.cache

rm -f dfa.dat
rm -f dfa.dot
rm -f final-build.out

rm -r ./loader/vmlinux.h
rm -f ./loader/monitor.o
rm -f ./loader/monitor.skel.h
rm -f ebpf-loader
