#ifndef ARCH_X86_IO_H
#define ARCH_X86_IO_H

#include <stdint.h>

/*
 * Write a byte to system IO port
 */
static inline void io_outb(const uint16_t port, const uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a byte from a system IO port
 */
static inline uint8_t io_inb(const uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Write a word to system IO port
 */
static inline void io_outw(const uint16_t port, const uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a word from a system IO port
 */
static inline uint16_t io_inw(const uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Write a dword to system IO port
 */
static inline void io_outl(const uint16_t port, const uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a dword from a system IO port
 */
static inline uint32_t io_inl(const uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Wait for a system IO operation to complete.
 */
static inline void io_wait(void) {
    // port 0x80 is used for 'checkpoints' during POST.
    // The Linux kernel seems to think it is free for use
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

#endif
