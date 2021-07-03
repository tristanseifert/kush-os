#include "MessageLoop.h"

const char *gLogTag = "dyldosrv";

/**
 * Entry point for the dynamic link server
 *
 * This will wait to receive mapping requests for a particular segment (represented by a program
 * header) in a library. If we have a matching segment it'll be returned; otherwise, we load it,
 * store it in the cache and return it.
 */
int main(const int argc, const char **argv) {
    MessageLoop rpc;
    rpc.run();

    return 0;
}
