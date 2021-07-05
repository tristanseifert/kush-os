#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

void _longjmp(jmp_buf env, int val) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    abort();
}

int _setjmp(jmp_buf env) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

void longjmp(jmp_buf env, int val) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    abort();
}

int setjmp(jmp_buf env) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}
