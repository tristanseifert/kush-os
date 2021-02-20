#include <stdio.h>
#include <sys/syscalls.h>

/**
 * Entry point for the dynamic link server
 */
int main(const int argc, const char **argv) {
    // set up environment
    TaskSetName(0, "dyldosrv");
    ThreadSetName(0, "Main");
    
    DbgOut("Megabitch", 9);
    
    while(1) {
        ThreadUsleep(1000 * 500);
    }
}
