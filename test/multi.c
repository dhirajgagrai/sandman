#include <stdio.h>
#include <stdlib.h>

int test() {
    printf("Wow\n");
    return 0;
}

int main(int argc, const char **argv) {
    int num;
    scanf("%i", &num);
    printf("number = %i\n", num);

    int x = 0;
    if (num) {
        x = rand();
    } else {
        printf("Enter test\n");
        test();
        printf("Exit test\n");
    }

    printf("%i\n", x);
    return 0;
}
