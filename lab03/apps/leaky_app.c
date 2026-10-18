#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== LAB-03: Valgrind Memory Debugging ===\n");

    // Bug 1: Memory leak
    char *message = malloc(100);
    strcpy(message, "Hello Embedded Linux");
    printf("Message: %s\n", message);

    // Bug 2: Invalid write
    int *numbers = malloc(5 * sizeof(int));

    for (int i = 0; i <= 5; i++) {
        numbers[i] = i * 10;
    }

    // Bug 3: Use after free
    free(numbers);
    printf("First number: %d\n", numbers[0]);

    printf("Done.\n");
    return 0;
}
