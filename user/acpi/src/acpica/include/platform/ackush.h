/******************************************************************************
 *
 * Name: ackush.h - OS specific defines, etc. for Kush
 *
 *****************************************************************************/
#ifndef __ACKUSH_H__
#define __ACKUSH_H__

/* Common (in-kernel/user-space) ACPICA configuration */

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_DO_WHILE_0
#define ACPI_IGNORE_PACKAGE_RESOLUTION_ERRORS

//#define ACPI_USE_SYSTEM_INTTYPES
//#define ACPI_USE_GPE_POLLING

// include library code
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

// int types
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

#if defined(__i386__)
#define ACPI_MACHINE_WIDTH          32
#else
#error Define ACPI_MACHINE_WIDTH for current arch
#endif

#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_NATIVE_MATH64

#define ACPI_DEBUG_OUTPUT

// no need to do anything weird to export symbols
#define ACPI_EXPORT_SYMBOL(symbol)

// spinlocks are implemented as booleans
#define ACPI_SPINLOCK               void *
// other lock types use the threads.h lock types
#define ACPI_MUTEX                  mtx_t *
#define ACPI_CPU_FLAGS              unsigned long
// we use ShittySemaphore
typedef struct __naive_sem naive_sem_t;
#define ACPI_SEMAPHORE              naive_sem_t *

#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "

#define ACPI_MSG_BIOS_ERROR     "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING   "ACPI BIOS Warning (bug): "

// sync objects
#define ACPI_MUTEX_TYPE             ACPI_OSL_MUTEX

// Use ACPI internal cache
#define ACPI_CACHE_T                ACPI_MEMORY_LIST
#define ACPI_USE_LOCAL_CACHE        1

#endif /* __ACKUSH_H__ */
