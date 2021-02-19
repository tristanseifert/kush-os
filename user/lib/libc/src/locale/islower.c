#include <ctype.h>

int islower(const int c) {
    return c >= 'a' && c <= 'z'; /* in the "C" locale */
}
