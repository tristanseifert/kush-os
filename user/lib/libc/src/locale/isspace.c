#include <ctype.h>

int isspace(const int c) {
    switch (c) { /* in the "C" locale: */
        case ' ':  /* space */
        case '\f': /* form feed */
        case '\n': /* new-line */
        case '\r': /* carriage return */
        case '\t': /* horizontal tab */
        case '\v': /* vertical tab */
            return 1;

        default:
            return 0;
    }
}
