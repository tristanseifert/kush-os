/*
 * Support for x86 processor exceptions
 */
#ifndef ARCH_X86_EXCEPTIONS_H
#define ARCH_X86_EXCEPTIONS_H

#include <stddef.h>
#include <stdint.h>

/**
 * Stack frame pushed by the assembly exception handler routines
 */
typedef struct x86_exception_info {
    uint32_t ds;                  // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t intNo, errCode;    // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} x86_exception_info_t;



/**
 * Installs the standard exception handlers.
 */
void exception_install_handlers();

/**
 * Formats an x86 exception info blob.
 */
int x86_exception_format_info(char *outBuf, const size_t outBufLen, 
        const x86_exception_info_t info);


/**
 * Handles a page fault exception.
 */
void x86_handle_pagefault(const x86_exception_info_t info);

#endif
