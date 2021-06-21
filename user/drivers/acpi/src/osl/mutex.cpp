/*
 * Implementations of the ACPICA OS layer: mutexes.
 */
#include "osl.h"
#include "log.h"

#include <cstdlib>
#include <threads.h>

#include <acpi.h>

/// whether mutex operations are logged
static bool gLogAcpicaMutex = false;

/**
 * Create a new mutex.
 */
ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle) {
    // allocate the mutex
    auto mtx = new mtx_t;
    if(!mtx) {
        return AE_NO_MEMORY;
    }

    int err = mtx_init(mtx, mtx_plain | mtx_recursive);

    if(gLogAcpicaMutex) Trace("AcpiOsCreateMutex %p", mtx);

    switch(err) {
        case thrd_success:
            *OutHandle = mtx;
            return AE_OK;

        case thrd_nomem:
            return AE_NO_MEMORY;
        default:
            return AE_ERROR;
    }
}

/**
 * Destroys a mutex.
 */
void AcpiOsDeleteMutex(ACPI_MUTEX mutex) {
    if(gLogAcpicaMutex) Trace("AcpiOsDeleteMutex %p", mutex);

    mtx_destroy(mutex);
    delete mutex;
}

/**
 * Acquires a mutex.
 *
 * XXX: Implement timeouts
 */
ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout) {
    if(gLogAcpicaMutex) Trace("AcpiOsAcquireMutex %p %u", Handle, Timeout);

    int err = mtx_lock(Handle);

    if(err == thrd_success) {
        return AE_OK;
    } else {
        return AE_ERROR;
    }
}

/**
 * Unlocks a mutex.
 */
void AcpiOsReleaseMutex(ACPI_MUTEX Handle) {
    if(gLogAcpicaMutex) Trace("AcpiOsReleaseMutex %p", Handle);

    mtx_unlock(Handle);
}
