#include "Syscalls.h"
#include "Handler.h"

#include "sched/ThreadState.h"

#include <handle/Manager.h>
#include <sched/Thread.h>
#include <sys/Syscall.h>
#include <arch/x86_io.h>
#include <log.h>

using namespace sys;
using namespace arch;

/// Port width ( converts to number of bytes )
enum class PortWidth {
    Byte                        = 1,
    Word                        = 2,
    DWord                       = 4
};

/// Perform an 8 bit wide port read/write
#define PORT_BYTE        (0x01)
/// Perform an 16 bit wide port read/write
#define PORT_WORD        (0x02)
/// Perform an 32 bit wide port read/write
#define PORT_DWORD       (0x03)
/// Bitmask for the port IO flags to get the port size in the flags field
#define PORT_SIZE_MASK   (0x0F)


/**
 * Updates the IO permission map for the given task.
 */
intptr_t syscall::UpdateIoPermission(const Handle taskHandle, const uint8_t *bitmap,
        const size_t numBits, const size_t portOffset, const uintptr_t flags) {
    rt::SharedPtr<sched::Task> task;

    constexpr static const size_t kBitmapTempSize = 32;
    uint8_t bitmapTemp[kBitmapTempSize];

    // validate buffer and copy in bitmap
    const size_t bitmapBytes = ((numBits + 7) / 8);
    if(!Syscall::validateUserPtr(bitmap, bitmapBytes)) {
        return Errors::InvalidPointer;
    } else if(bitmapBytes > kBitmapTempSize) {
        return Errors::BufferTooLarge;
    }

    if(portOffset + numBits > 0xFFFF) {
        return Errors::InvalidArgument;
    }

    Syscall::copyIn(bitmap, bitmapBytes, &bitmapTemp, kBitmapTempSize);

    // get the appropriate task
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // ensure the map hasn't been locked, and initialize it if needed
    auto &state = task->archState;
    if(state.ioBitmapLocked) {
        return Errors::PermissionDenied;
    }

    if(!state.ioBitmap) {
        if(!state.initIoPermissions()) {
            return Errors::GeneralError;
        }
    }

    for(size_t i = 0; i < numBits; i++) {
        // test the bit in the bitmap
        const bool allow = bitmapTemp[i/8] & (1 << (i % 8));

        if(allow) {
            if(!state.allowIoRange(portOffset + i, 1)) {
                return Errors::GeneralError;
            }
        }
        // XXX: if not set, should we remove permission for the port?
    }

    // we're done here
    return Errors::Success;
}

/**
 * Locks the IO permissions bitmap of the given task.
 */
intptr_t syscall::LockIoPermission(const Handle taskHandle) {
    rt::SharedPtr<sched::Task> task;

    // get the task
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // lock it
    auto &state = task->archState;
    state.lockIoPermissions();

    return Errors::Success;
}

/**
 * Performs an IO port read on behalf of the calling task.
 */
intptr_t syscall::IoPortRead(const uintptr_t port, const uintptr_t flags, uint32_t *outValue) {
    // validate port, flags (for width) and out ptr
    if(port > 0xFFFF) {
        return Errors::InvalidArgument;
    }

    const auto widthMask = flags & PORT_SIZE_MASK;
    PortWidth width;

    switch(widthMask) {
        case PORT_BYTE:
            width = PortWidth::Byte;
            break;
        case PORT_WORD:
            width = PortWidth::Word;
            break;
        case PORT_DWORD:
            width = PortWidth::DWord;
            break;
        default:
            return Errors::InvalidArgument;
    }

    if(!Syscall::validateUserPtr(outValue, sizeof(*outValue))) {
        return Errors::InvalidPointer;
    }

    // is the current task authorized to perform this IO?
    auto &state = sched::Task::current()->archState;
    if(!state.testIoRange(port, static_cast<uint16_t>(width))) {
        return Errors::PermissionDenied;
    }

    // perform read
    uint32_t temp = 0;
    switch(width) {
        case PortWidth::Byte:
            temp = io_inb(port);
            break;
        case PortWidth::Word:
            temp = io_inw(port);
            break;
        case PortWidth::DWord:
            temp = io_inl(port);
            break;
    }

    Syscall::copyOut(&temp, sizeof(temp), outValue, sizeof(*outValue));
    return Errors::Success;
}

/**
 * Performs an IO port write on behalf of the calling task.
 */
intptr_t syscall::IoPortWrite(const uintptr_t port, const uintptr_t flags, const uint32_t value) {
    // validate port, flags (for width) and out ptr
    if(port > 0xFFFF) {
        return Errors::InvalidArgument;
    }

    const auto widthMask = flags & PORT_SIZE_MASK;
    PortWidth width;

    switch(widthMask) {
        case PORT_BYTE:
            width = PortWidth::Byte;
            break;
        case PORT_WORD:
            width = PortWidth::Word;
            break;
        case PORT_DWORD:
            width = PortWidth::DWord;
            break;
        default:
            return Errors::InvalidArgument;
    }

    // is the current task authorized to perform this IO?
    auto &state = sched::Task::current()->archState;
    if(!state.testIoRange(port, static_cast<uint16_t>(width))) {
        return Errors::PermissionDenied;
    }

    // perform write
    switch(width) {
        case PortWidth::Byte:
            io_outb(port, value & 0xFF);
            break;
        case PortWidth::Word:
            io_outw(port, value & 0xFFFF);
            break;
        case PortWidth::DWord:
            io_outl(port, value);
            break;
    }

    return Errors::Success;
}
