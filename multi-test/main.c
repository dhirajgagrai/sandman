// main.c
#include <stdio.h>
#include <stdlib.h>

void helper_read_file();
void helper_execute();
void helper_exit();

int main() {
    printf("Main: Program starting.\n");

    helper_read_file();

    helper_execute();
    
    printf("Main: Program finishing.\n");
    helper_exit();

    return 0;
}
