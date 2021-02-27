/*
 * Implementations of the ACPICA OS layer, for functions to do with threads.
 */
#include "osl.h"
#include "log.h"

#include <threads.h>
#include <sys/syscalls.h>

#include <acpi.h>

/**
 * Return the current thread id.
 */
ACPI_THREAD_ID AcpiOsGetThreadId() {
    /// thrd_current() reutrns a pointer; ACPI_THREAD_ID is guaranteed to fit a pointer
    return (ACPI_THREAD_ID) thrd_current();
}

/**
 * Creates a new thread.
 */
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
    Trace("AcpiOsExecute: type %u function %p ctx %p", Type, (void *) Function, Context);

    int err;

    if(!Function) {
        return AE_BAD_PARAMETER;
    }

    thrd_t id;
    /// XXX: shouldn't cast function pointers, but only the return type changes...
    err = thrd_create(&id, reinterpret_cast<int(*)(void *)>(Function), Context);

    switch(err) {
        case thrd_success:
            return AE_OK;

        case thrd_nomem:
            return AE_NO_MEMORY;
        default:
            return AE_ERROR;
    }
}

/**
 * Waits for all threads created from AcpiOsExecute() to complete.
 */
void AcpiOsWaitEventsComplete(void) {
    // TODO: implement?
}


/**
 * Sleep for the given number of milliseconds.
 */
void AcpiOsSleep(UINT64 Milliseconds) {
    ThreadUsleep(Milliseconds * 1000);
}

/**
 * Perform a busy wait, without putting the thread to sleep.
 */
void AcpiOsStall(UINT32 Microseconds) {
    // TODO: implement
    ThreadUsleep(Microseconds);
}
