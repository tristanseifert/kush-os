#include "../sys/syscall.h"
#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>

#include <stdint.h>
#include <stdbool.h>

/**
 * Updates the base of the %fs register for the thread.
 */
int Amd64SetThreadLocalBase(const int which, const uintptr_t base) {
    return Amd64SetThreadLocalBaseFor(0, which, base);
}

/**
 * Updates the base of either the %fs or %gs register,
 */
int Amd64SetThreadLocalBaseFor(const uintptr_t threadHandle, const int which, const uintptr_t base) {
    return __do_syscall3(threadHandle, (which == SYS_ARCH_AMD64_TLS_GS) ? true : false, base,
            SYS_ARCH_AMD64_SET_FGS_BASE);
}

/**
 * Copies out bootloader information into the provided user buffer.
 *
 * @return Number of bytes copied, or negative error code.
 */
int Amd64CopyLoaderInfo(void *outBuf, const size_t outBufLen) {
    return __do_syscall2((uintptr_t) outBuf, outBufLen, SYS_ARCH_AMD64_GET_LOADER_INFO);
}


/**
 * Performs a read of an IO port.
 *
 * @return 0 on success, error code otherwise.
 */
int Amd64PortRead(const uintptr_t port, const uintptr_t flags, uint32_t *read) {
    if(!flags || !read) {
        return -1;
    }

    return __do_syscall3(port, flags, (uintptr_t) read, SYS_ARCH_AMD64_PORT_READ);
}
int Amd64PortReadB(const uintptr_t port, const uintptr_t flags, uint8_t *read) {
    // validate input ptr
    if(!read) {
        return -1;
    }
    // perform IO
    int err;
    uint32_t temp;
    if((err = Amd64PortRead(port, 
        ((flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_BYTE), &temp))) {
        return err;
    }
    *read = temp & 0xFF;
    return 0;
}

int Amd64PortReadW(const uintptr_t port, const uintptr_t flags, uint16_t *read) {
    // validate input ptr
    if(!read) {
        return -1;
    }
    // perform IO
    int err;
    uint32_t temp;
    if((err = Amd64PortRead(port, 
        ((flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_WORD), &temp))) {
        return err;
    }
    *read = temp & 0xFFFF;
    return 0;
}

int Amd64PortReadD(const uintptr_t port, const uintptr_t flags, uint32_t *read) {
    return Amd64PortRead(port, 
            (flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_DWORD, read);
}



/**
 * Performs a write to an IO port.
 *
 * @return 0 on success, error code otherwise.
 */
int Amd64PortWrite(const uintptr_t port, const uintptr_t flags, const uint32_t write) {
    if(!flags) {
        return -1;
    }

    return __do_syscall3(port, flags, write, SYS_ARCH_AMD64_PORT_WRITE);
}
int Amd64PortWriteB(const uintptr_t port, const uintptr_t flags, const uint8_t write) {
    return Amd64PortWrite(port, 
            (flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_BYTE, write);
}
int Amd64PortWriteW(const uintptr_t port, const uintptr_t flags, const uint16_t write) {
    return Amd64PortWrite(port, 
            (flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_WORD, write);
}
int Amd64PortWriteL(const uintptr_t port, const uintptr_t flags, const uint32_t write) {
    return Amd64PortWrite(port, 
            (flags & ~SYS_ARCH_AMD64_PORT_SIZE_MASK) | SYS_ARCH_AMD64_PORT_DWORD, write);
}


/**
 * Updates a subset of the IO permission bitmap for the specified task.
 */
int Amd64UpdateAllowedIoPortsFor(const uintptr_t taskHandle, const void *bitmap,
        const uintptr_t numBits, const uintptr_t offset) {
    // build the offset/length arg
    if(numBits > 65536 || (offset + numBits) > 65536) {
        return -1;
    }

    // perform call
    return __do_syscall4(taskHandle, (uintptr_t) bitmap, numBits, offset, SYS_ARCH_AMD64_PORT_ALLOWLIST);
}

/**
 * Update the IO permission bitmap for the current task.
 */
int Amd64UpdateAllowedIoPorts(const void *bitmap, const size_t numBits, const uintptr_t portOffset) {
    return Amd64UpdateAllowedIoPortsFor(0, bitmap, numBits, portOffset);
}



/**
 * Called when a thread returns; it'll gracefully destroy this one.
 */
void LIBSYSTEM_INTERNAL Amd64ThreadExit() {
    int err;
    uintptr_t handle;

    // get handle
    err = ThreadGetHandle(&handle);
    if(err) {
        asm volatile("ud2" ::: "memory");
    }

    // then destroy
    ThreadDestroy(handle);
}
