/*
 * Support for x86_64 processor exceptions
 */
#ifndef ARCH_X64_EXCEPTIONS_H
#define ARCH_X64_EXCEPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>
#include <x86intrin.h>

/**
 * Stack frame pushed by the assembly exception handler routines
 */
typedef struct amd64_exception_info {
    // SSE registers
    // __m128i xmm[8];

    // registers added for 64-bit mode
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    // basic registers (in common with 32-bit mode, in same order as PUSHA)
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;

    // pushed by exception handler: 0-31 are exceptions
    uint64_t intNo;
    // pushed for exceptions; other traps push a dummy value
    uint64_t errCode;

    // pushed by processor as exception handler
    uint64_t rip, cs, rflags;
    // pushed by processor when crossing rings
    uint64_t rsp, ss;
} __attribute__((packed)) amd64_exception_info_t;



/**
 * Installs the standard exception handlers.
 */
void exception_install_handlers();

/**
 * Formats an x86_64 exception info blob.
 */
int amd64_exception_format_info(char *outBuf, const size_t outBufLen, 
        const amd64_exception_info_t *info);


/**
 * Handles a page fault exception.
 */
void amd64_handle_pagefault(amd64_exception_info_t *info);

/**
 * Handles all other (e.g. not page fault) exceptions.
 */
void amd64_handle_exception(amd64_exception_info_t *info);

#ifdef __cplusplus
}
#endif
#endif
