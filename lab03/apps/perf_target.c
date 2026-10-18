#include <stdio.h>
#include <stdint.h>

int main(void) {
    volatile uint64_t sum = 0;

    printf("=== LAB-03: Performance Profiling ===\n");

    for (uint64_t i = 0; i < 100000000ULL; i++) {
        sum += (i * i) % 97;
    }

    printf("Result: %llu\n", (unsigned long long)sum);

    return 0;
}
