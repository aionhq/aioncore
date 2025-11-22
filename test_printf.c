#include <stdint.h>
#include <stdio.h>

#define PAGE_SIZE 4096

int main() {
    printf("Test 1 - PAGE_SIZE with %%u: %u\n", PAGE_SIZE);
    printf("Test 2 - PAGE_SIZE with %%lu: %lu\n", PAGE_SIZE);
    printf("Test 3 - PAGE_SIZE cast to unsigned int with %%u: %u\n", (unsigned int)PAGE_SIZE);

    uint32_t val = 4096;
    printf("Test 4 - uint32_t with %%u: %u\n", val);
    printf("Test 5 - uint32_t with %%lu: %lu\n", val);
    printf("Test 6 - uint32_t cast to unsigned int with %%u: %u\n", (unsigned int)val);

    return 0;
}
