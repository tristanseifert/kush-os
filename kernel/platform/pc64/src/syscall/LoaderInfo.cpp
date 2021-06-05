#include <sys/Syscall.h>

#include <stdint.h>
#include <arch.h>
#include <bootboot.h>

// forward declare the function as it is in the arch Syscalls.h file
namespace arch::syscall {
intptr_t GetLoaderInfo(void *outBuf, const size_t outBufLen);
}


extern "C" BOOTBOOT bootboot;

using namespace sys;

/**
 * Copies out the bootloader information structure.
 */
intptr_t arch::syscall::GetLoaderInfo(void *outBuf, const size_t outBufLen) {
    // validate the buffer ptr
    if(outBufLen < sizeof(BOOTBOOT)) {
        return Errors::BufferTooSmall;
    }
    else if(!Syscall::validateUserPtr(outBuf, outBufLen)) {
        return Errors::InvalidPointer;
    }

    // copy out the structure. we'll always copy this amount of bytes
    Syscall::copyOut(&bootboot, sizeof(bootboot), outBuf, outBufLen);
    return sizeof(bootboot);
}

