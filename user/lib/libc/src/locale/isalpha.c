#include <ctype.h>

int isalpha(const int c) {
    return isupper(c) || islower(c); /* in the "C" locale */
}
