#include <string.h>

/*
 * Compares n bytes of the two strings.
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    while(n--) {
        if(*s1++!=*s2++) {
            return *(const unsigned char*)(s1 - 1) - *(const unsigned char*)(s2 - 1);
        }
    }

    return 0;
}
