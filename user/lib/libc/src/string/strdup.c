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
