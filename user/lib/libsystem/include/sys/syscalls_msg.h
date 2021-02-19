#ifndef _LIBSYSTEM_SYSCALLS_MSG_H
#define _LIBSYSTEM_SYSCALLS_MSG_H

/**
 * Header for a receive message buffer, populated by the kernel with auxiliary information
 * about a received message.
 *
 * @note It is critical these are always 16-byte aligned!
 */
typedef struct MessageHeader {
    /// handle of the thread that sent this message
    uintptr_t sender;
    /// flags (not currently used)
    uint16_t flags;
    /// number of bytes of message data
    uint16_t receivedBytes;

    uintptr_t reserved[2];

    /// message data
    uint8_t data[];
} MessageHeader_t __attribute__((aligned(16)));

LIBSYSTEM_EXPORT int PortCreate(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int PortDestroy(const uintptr_t portHandle);
LIBSYSTEM_EXPORT int PortSend(const uintptr_t portHandle, const void *message, const size_t messageLen);
LIBSYSTEM_EXPORT int PortReceive(const uintptr_t portHandle, MessageHeader_t *buf,
        const size_t bufMaxLen, const uintptr_t blockUs);
LIBSYSTEM_EXPORT int PortSetQueueDepth(const uintptr_t portHandle, const uintptr_t queueDepth);


#endif
