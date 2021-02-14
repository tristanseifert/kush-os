#include <sys/syscalls.h>
#include <string.h>

#include "log.h"

int main(int argc, const char **argv) {
    // initialize environment
    TaskSetName(0, "rootsrv");
    ThreadSetName(0, "Main");

    int taskHandle = TaskGetHandle();
    int threadHandle = ThreadGetHandle();

    log("task handle %08x; thread handle %08x", taskHandle, threadHandle);

    // parse the init bundle

    // start the message handler thread

    // launch all the loaded drivers

    // then the servers

    // wait for quit notification
    while(1) {
        ThreadUsleep(420 * 1000);
    }
}
