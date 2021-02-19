#include <string.h>

/*
 * Copies n bytes from the source string to the destination buffer, filling the
 * destination with zeros if source ends prematurely.
 */
char *strncpy(char *dest, const char *src, size_t n) {
    char *ret = dest;
    do {
        if (!n--) {
            return ret;
        }
    } while ((*dest++ = *src++));

    while (n--) {
        *dest++ = 0;
    }

    return ret;
}
