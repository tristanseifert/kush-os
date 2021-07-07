#include "Syscalls.h"
#include "Handler.h"

#include <sys/Syscall.h>
#include <platform.h>
#include <log.h>

using namespace sys;
using namespace arch;


/**
 * Enables or disables the kernel's framebuffer console.
 */
intptr_t syscall::SetFbConsState(const bool enabled) {
    int err = platform::SetConsoleState(enabled);
    if(err) {
        log("failed to set fb state: %d", err);
    }

    return err ? Errors::GeneralError : Errors::Success;
}

