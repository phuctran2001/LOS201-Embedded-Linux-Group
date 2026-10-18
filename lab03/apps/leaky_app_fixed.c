#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== LAB-03: Valgrind Memory Debugging ===\n");

    char *message = malloc(100);
    int *numbers = malloc(5 * sizeof(int));

    if (message == NULL || numbers == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free(message);
        free(numbers);
        return 1;
    }

    strcpy(message, "Hello Embedded Linux");
    printf("Message: %s\n", message);

    for (int i = 0; i < 5; i++) {
        numbers[i] = i * 10;
    }

    printf("First number: %d\n", numbers[0]);

    free(numbers);
    free(message);

    printf("Done.\n");
    return 0;
}
