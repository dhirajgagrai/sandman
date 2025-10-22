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
        exit(1);
    } else {
        test();
    }

    printf("%i\n", num + 2);
    return 0;
}
