#include <string.h>
#include <stdint.h>

/**
 * Moves the given memory region; they can overlap. A temporary stack buffer is used.
 */
void *memmove(void *dest, const void *src, const size_t n) {
    unsigned char tmp[n];
    memcpy(tmp, src, n);
    memcpy(dest, tmp, n);
    return dest;
}
