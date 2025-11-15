#include <stdio.h>
#include <stdlib.h>

int test() {
    printf("Wow\n");
    exit(1);
}

int main(int argc, const char **argv) {
    int num;
    scanf("%i", &num);

    if (num) {
        printf("TRUE\n");
    } else {
        test();
    }

    printf("number = %i\n", num);
    return 0;
}
