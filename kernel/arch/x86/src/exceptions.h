/*
 * Support for x86 processor exceptions
 */
#ifndef ARCH_X86_EXCEPTIONS_H
#define ARCH_X86_EXCEPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

/**
 * Stack frame pushed by the assembly exception handler routines
 */
typedef struct x86_exception_info {
    // segment selectors
    uint32_t gs, fs, es, ds;

    // registers from PUSHA; esp there is useless
    uint32_t edi, esi, ebp, oesp, ebx, edx, ecx, eax;

    // pushed by exception handler: 0-31 are exceptions
    uint32_t intNo;
    // pushed for exceptions; other traps push a dummy value
    uint32_t errCode;

    // pushed by processor as exception handler
    uint32_t eip, cs, eflags;
    // pushed by processor when crossing rings
    uint32_t esp, ss;
} x86_exception_info_t;



/**
 * Installs the standard exception handlers.
 */
void exception_install_handlers();

/**
 * Formats an x86 exception info blob.
 */
int x86_exception_format_info(char *outBuf, const size_t outBufLen, 
        const x86_exception_info_t &info);


/**
 * Handles a page fault exception.
 */
void x86_handle_pagefault(const x86_exception_info_t info);

/**
 * Handles all other (e.g. not page fault) exceptions.
 */
void x86_handle_exception(const x86_exception_info_t info);

#ifdef __cplusplus
}
#endif
#endif
