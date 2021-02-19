#include <ctype.h>

int ispunct(const int c) {
    return isprint(c) && !isspace(c) && !isalnum(c); /* in the "C" locale */
}
