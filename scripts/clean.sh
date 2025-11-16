#!/bin/bash

rm -rf ./build/*
rm -rf ./.cache

rm -f nfa.dat
rm -f nfa.dot
rm -f final-build.out

rm -f ./loader/vmlinux.h
rm -f ./loader/monitor.o
rm -f ./loader/monitor.skel.h
rm -f ebpf-loader
