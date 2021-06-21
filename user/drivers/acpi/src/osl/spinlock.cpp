/*
 * Implementations of the ACPICA OS layer: spinlocks
 *
 * These are implemented as compiler intrinsics on top of bool pointers.
 */
#include <acpi.h>

#include <atomic>

/**
 * Allocate a new spinlock.
 */
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    // allocate memory
    auto flag = new std::atomic_flag;
    if(!flag) {
        return AE_NO_MEMORY;
    }

    *OutHandle = reinterpret_cast<void *>(flag);

    return AE_OK;
}

/**
 * Releases a previously allocated spinlock flag.
 */
void AcpiOsDeleteLock(ACPI_SPINLOCK lock) {
    auto flag = reinterpret_cast<std::atomic_flag *>(lock);
    delete flag;
}

/**
 * Attempt to acquire a spinlock.
 *
 * TODO: Implement support for timeouts.
 *
 * @return Some arbitrary value to pass to AcpiOsReleaseLock() again
 */
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle) {
    auto flag = reinterpret_cast<std::atomic_flag *>(handle);

    while(flag->test_and_set(std::memory_order_acquire)) {
        // nothing
    }

    return 0;
}

/**
 * Release spinlock.
 *
 * @param flags Return value from AcpiOsAcquireLock().
 */
void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags) {
    auto flag = reinterpret_cast<std::atomic_flag *>(handle);
    flag->clear(std::memory_order_release);
}
