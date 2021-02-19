#include <ctype.h>

int isblank(const int c) {
    return c == ' ' || c == '\t'; /* in the "C" locale */
}
