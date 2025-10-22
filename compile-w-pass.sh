#!/bin/bash

clang -fpass-plugin=build/pass/Dummy.so ./test/example.c -o ./test/example.out
