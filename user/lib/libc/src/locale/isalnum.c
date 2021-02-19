#include <sys/cdefs.h>
#include <ctype.h>

int isalnum(const int c) {
    return isalpha(c) || isdigit(c);
}
