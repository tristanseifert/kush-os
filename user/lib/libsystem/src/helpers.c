#include "helpers.h"

#include <stdint.h>

void *InternalMemset(void *_ptr, int c, size_t len) {
    uint8_t *ptr = _ptr;

    for(size_t i = 0; i < len; i++) {
        *ptr++ = c;
    }

    return _ptr;
}

size_t InternalStrlen(const char *s) {
    size_t len = 0;

    while(*s++) {
        len++;
    }

    return len;
}

