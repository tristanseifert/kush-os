#include <string.h>
#include <stdint.h>

/*
 * Finds the first occurrence of value in the first num bytes of ptr.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"

void *memchr(const void *ptr, const int value, const size_t num) {
    const uint8_t *read = (const uint8_t *) ptr;

    for(int i = 0; i < num; i++) {
        if(read[i] == value) return (void *) &read[i];
    }

    return NULL;
}

#pragma clang diagnostic pop
