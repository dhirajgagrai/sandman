#include <stdio.h>
#include <stdlib.h>

int test() {
    printf("Wow\n");
    return 0;
}

int main(int argc, const char **argv) {
    int num;
    scanf("%i", &num);

    if (num) {
        printf("TRUE\n");
    } else {
        int x = rand();
        printf("num + random: %i\n", num + x);
    }

    printf("number = %i\n", num);
    return 0;
}
