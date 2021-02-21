#include <stdlib.h>
#include <locale.h>

// TODO: proper implementations

long strtol_l(const char *nptr, char **endptr, int base, locale_t loc) {
    return strtol(nptr, endptr, base);
}

long long strtoll_l(const char *nptr, char **endptr, int base, locale_t loc) {
    return strtoll(nptr, endptr, base);
}

unsigned long strtoul_l(const char *nptr, char **endptr, int base, locale_t loc) {
    return strtoul(nptr, endptr, base);
}

unsigned long long strtoull_l(const char *nptr, char **endptr, int base, locale_t loc) {
    return strtoull(nptr, endptr, base);
}

double strtod_l(const char *nptr, char **endptr, locale_t loc) {
    return strtod(nptr, endptr);
}

float strtof_l(const char *nptr, char **endptr, locale_t loc) {
    return strtof(nptr, endptr);
}

long double strtold_l(const char *nptr, char **endptr, locale_t loc) {
    return strtold(nptr, endptr);
}
