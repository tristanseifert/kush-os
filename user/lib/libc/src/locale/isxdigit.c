#include <ctype.h>
#include <locale.h>

int isxdigit(const int c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

// TODO: proper implementation
int isxdigit_l(int c, locale_t loc) {
    return isxdigit(c);
}
