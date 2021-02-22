#include <stdio.h>
#include <sys/syscalls.h>

static int fuck = 0;

/**
 * Entry point for the dynamic link server
 */
int main(const int argc, const char **argv) {
    // set up environment
    DbgOut("Megabitch has secreted", 22);

    fuck = 2;

    fprintf(stderr, "fuck value %d\n", fuck);

    while(1) {
        ThreadUsleep(1000 * 500);
    }
}
