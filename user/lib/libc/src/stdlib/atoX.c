/**
 * Various wrappers around the old style atoX methods that call into the new, modern versions that
 * are for the most part provided by gdtoa and the rest of the library.
 */
#include <stdlib.h>

int atoi(const char *str) {
    return (int) strtol(str, NULL, 10);
}

long atol(const char *str) {
    return strtol(str, NULL, 10);
}
long long atoll(const char *str) {
    return strtoll(str, NULL, 10);
}

double atof(const char *str) {
    return strtod(str, NULL);
}
