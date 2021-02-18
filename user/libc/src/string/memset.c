#include <string.h>
#include <stdint.h>

/*
 * Fills a given segment of memory with a specified value.
 */
void *memset(void *ptr, const int value, const size_t num) {
    if(value == 0x00) {
        return memclr(ptr, num);
    }

    uint8_t *write = (uint8_t *) ptr;

    for(int i = 0; i < num; i++) {
        write[i] = value;
    }

    return ptr;
}
