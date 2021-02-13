#include <string.h>
#include <stdint.h>

/*
 * Finds the first occurrence of value in the first num bytes of ptr.
 */
const void *memchr(const void *ptr, const uint8_t value, const size_t num) {
    const uint8_t *read = (const uint8_t *) ptr;

    for(int i = 0; i < num; i++) {
        if(read[i] == value) return &read[i];
    }

    return NULL;
}
