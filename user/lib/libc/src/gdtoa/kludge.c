/**
 * Kludge together some not quite right implementations for routines we don't currently
 * implement for one reason or another.
 */
#include <stdlib.h>

long double strtold (const char* str, char** endptr) {
    return strtod(str, endptr);
}

// wide char functions not yet implemented!
float wcstof (const wchar_t* str, wchar_t** endptr) {
    abort();
}
double wcstod (const wchar_t* str, wchar_t** endptr) {
    abort();
}
long double wcstold (const wchar_t* str, wchar_t** endptr) {
    abort();
}

