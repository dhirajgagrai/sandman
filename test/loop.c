#include <stdio.h>

int test() {
    printf("Wow\n");
    return 0;
}

int main(int argc, const char **argv) {
    int num;
    scanf("%i", &num);

    while (num) {
        test();
        scanf("%i", &num);
    }

    printf("number = %i\n", num);
    return 0;
}
