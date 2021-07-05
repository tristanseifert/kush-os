#include <stdio.h>

int puts(const char *s) {
    int temp = fputs(s, stdout);
    if(temp == EOF) return temp;

    fputc('\n', stdout);

    return temp;
}

int putchar(int c) {
    return fputc(c, __stdoutp);
}
