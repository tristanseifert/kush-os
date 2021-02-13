#include <string.h>
#include <stdint.h>

/*
 * Copies num bytes from source to destination.
 */
void *memcpy(void *destination, const void *source, const size_t num) {
    uint32_t *dst = (uint32_t *) destination;
    const uint32_t *src = (const uint32_t *) source;

    int i = 0;
    for(i = 0; i < num/4; i++) {
        *dst++ = *src++;
    }

    // If we have more bytes to copy, perform the copy
    if((i * 4) != num) {
        uint8_t *dst_byte = (uint8_t *) dst;
        const uint8_t *src_byte = (const uint8_t *) src;

        for(int x = (i * 4); x < num; x++) {
            *dst_byte++ = *src_byte++;
        }
    }

    return destination;
}
