#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

// TODO: proper implementations
int strcoll_l(const char *s1, const char *s2, locale_t loc) {
    return strcoll(s1, s2);
}
