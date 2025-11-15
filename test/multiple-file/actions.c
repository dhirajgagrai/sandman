#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void do_open() {
    printf(0);
}

void do_read() {
    printf("Action: Reading file...\n");
}

void do_exec() {
    printf("Action: Executing new process...\n");
}

void do_exit() {
    exit(1);
    // This should not be present in the minimized NFA
    printf("Action: Exiting...\n");
}
