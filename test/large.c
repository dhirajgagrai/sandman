#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int calculate_sum(int max) {
    int sum = 0;
    for (int i = 0; i < max; i++) {
        if (i % 2 == 0) {
            printf("Summing even number: %d\n", i);
            sum += i;
        } else {
            printf("Skipping odd number: %d\n", i);
        }
    }

    if (sum > 10) {
        puts("Sum is large.");
    }

    return sum;
}

void process_data(int mode) {
    switch (mode) {
    case 1: {
        puts("Mode 1: Allocating memory.");
        void *ptr = malloc(100);
        printf("Address: %p\n", ptr);
        memset(ptr, 0, 100);
        free(ptr);
        puts("Mode 1: Complete.");
        break;
    }
    case 2: {
        puts("Mode 2: Running calculation.");
        int result = calculate_sum(5);
        printf("Mode 2: Calculation result: %d\n", result);
        break;
    }
    case 3:
        puts("Mode 3: Just printing.");
        break;
    default:
        puts("Invalid mode.");
        break;
    }
}

int main() {
    int a, b;

    printf("--- Program Start ---\n");
    puts("Enter number 'a':");
    scanf("%d", &a);
    puts("Enter number 'b':");
    scanf("%d", &b);
    printf("Read a=%d, b=%d\n", a, b);

    if (a > b) {
        printf("Branch A: a > b. Calling process_data(1)\n");
        process_data(1);
        printf("Branch A: Returned from process_data(1)\n");
    } else {
        printf("Branch B: a <= b. Calling process_data(2)\n");
        process_data(2);
        printf("Branch B: Returned from process_data(2)\n");
    }

    int k = 0;
    while (k < 2) {
        printf("Main loop, iteration %d\n", k);

        for (int j = 0; j < 2; j++) {
            printf("...Nested loop %d\n", j);
            int r = rand();
            if (r % 2 == 0) {
                puts("......Random is even.");
            }
        }

        k++;
    }

    process_data(a % 4);

    puts("--- Program End ---");
    return 0;
}
