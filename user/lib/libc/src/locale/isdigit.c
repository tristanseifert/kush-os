#include <ctype.h>
#include <locale.h>

int isdigit(const int c) {
    return c >= '0' && c <= '9';
}

// TODO: proper implementation
int isdigit_l(int c, locale_t loc) {
    return isdigit(c);
}
