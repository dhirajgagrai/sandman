#include <stdarg.h> // For va_list
#include <stdint.h> // For uint64_t
#include <stdio.h>
#include <string.h> // For memset, memcpy

/**
 * @brief Variadic function to generate va_list intrinsics.
 *
 * This function will generate:
 * - @llvm.va_start
 * - @llvm.va_arg
 * - @llvm.va_end
 */
int sum_variadic(int count, ...) {
    int total = 0;
    va_list args;

    // Will be lowered to @llvm.va_start
    va_start(args, count);

    for (int i = 0; i < count; ++i) {
        // va_arg is often inlined, but its logic relies on
        // the va_list pointer from va_start
        total += va_arg(args, int);
    }

    // Will be lowered to @llvm.va_end
    va_end(args);

    return total;
}

int main() {
    // --- 1. Memory Intrinsics ---
    char buffer[100];
    char src[] = "Hello Intrinsics!";

    // Will be lowered to @llvm.memset.*
    // (e.g., @llvm.memset.p0.i64)
    memset(buffer, 0, 100);

    // Will be lowered to @llvm.memcpy.*
    // (e.g., @llvm.memcpy.p0.p0.i64)
    memcpy(buffer, src, strlen(src) + 1);

    printf("Buffer: %s\n", buffer);

    // --- 2. Bit Manipulation Intrinsics ---
    uint64_t bits = 0x010101010000FFFF;

    // Will be lowered to @llvm.ctpop.i64 (Count Population)
    int pop_count = __builtin_popcountll(bits);

    // Will be lowered to @llvm.clz.i64 (Count Leading Zeros)
    int lead_zeros = __builtin_clzll(bits);

    // Will be lowered to @llvm.cttz.i64 (Count Trailing Zeros)
    int tail_zeros = __builtin_ctzll(bits);

    printf("Bit counts: pop=%d, clz=%d, ctz=%d\n",
           pop_count, lead_zeros, tail_zeros);

    // --- 3. Overflow-Checked Arithmetic Intrinsics ---
    int a = 2000000000;
    int b = 2000000000;
    int result;

    // Will be lowered to @llvm.sadd.with.overflow.i32
    int overflowed = __builtin_sadd_overflow(a, b, &result);

    printf("Overflow check: %d, Result: %d\n", overflowed, result);

    // --- 4. Branch Hint Intrinsic ---
    // Use volatile to prevent optimizer from removing the check
    volatile int x = 5;

    // Will be lowered to @llvm.expect.i32
    if (__builtin_expect(x > 0, 1)) { // 1 = likely true
        printf("Likely path taken.\n");
    } else {
        printf("Unlikely path taken.\n");
    }

    // --- 5. Call the variadic function ---
    int v_sum = sum_variadic(4, 10, 20, 30, 40);
    printf("Variadic sum: %d\n", v_sum);

    return 0;
}
