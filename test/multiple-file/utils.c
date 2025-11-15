#include <stdio.h>

void do_open();
void do_read();
void do_exec();
void do_exit();

void helper_read_file() {
    printf("Util: Preparing to read...\n");
    do_open();
    do_read();
    printf("Util: Done reading.\n");
}

void helper_execute() {
    printf("Util: Preparing to execute...\n");
    do_exec();
}

void helper_exit() {
    printf("Util: Preparing to exit...\n");
    do_exit();
}
