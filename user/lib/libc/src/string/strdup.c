#include <string.h>
#include <stdlib.h>
#include <malloc.h>

char *strdup(const char *in) {
    size_t len = strlen(in);
    void *dest = calloc((len + 1), sizeof(char));
    if(!dest) {
        return NULL;
    }

    memcpy(dest, in, len);
    return dest;
}

/**
 * Same as strdup but with a maximum length
 */
char *strndup(const char *in, size_t maxlen) {
    size_t len = strnlen(in, maxlen);
    char *copy = malloc(len + 1);

    if(copy) {
        memcpy(copy, in, len);
        copy[len] = '\0';
    }

    return copy;
}
