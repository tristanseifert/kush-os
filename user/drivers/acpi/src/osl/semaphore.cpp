/*
 * Implementations of the ACPICA OS layer
 */
#include "osl.h"
#include "log.h"

#include <acpi.h>

#include <stdatomic.h>
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

typedef struct __naive_sem {
    atomic_int val;   // int is plenty big.  Must be signed, so an extra decrement doesn't make 0 wrap to >= 1
    atomic_int max;
} naive_sem_t;

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
static inline void spinloop_body(void) { _mm_pause(); }  // PAUSE is "rep nop" in asm output
#else
static inline void spinloop_body(void) { }
#endif

void sem_down(naive_sem_t *sem)
{
  while (1) {
    while (likely(atomic_load_explicit(&(sem->val), memory_order_acquire ) < 1))
        spinloop_body();  // wait for a the semaphore to be available
    int tmp = atomic_fetch_add_explicit( &(sem->val), -1, memory_order_acq_rel );  // try to take the lock.  Might only need mo_acquire
    if (likely(tmp >= 1))
        break;              // we successfully got the lock
    else                    // undo our attempt; another thread's decrement happened first
        atomic_fetch_add_explicit( &(sem->val), 1, memory_order_release ); // could be "relaxed", but we're still busy-waiting and want other thread to see this ASAP
  }
}
// note the release, not seq_cst.  Use a stronger ordering parameter if you want it to be a full barrier.
void sem_up(naive_sem_t *sem) {
    atomic_fetch_add_explicit(&(sem->val), 1, memory_order_release);
}

/**
 * Creates a new semaphore.
 */
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 current, ACPI_SEMAPHORE *outHandle) {
    Trace("AcpiOsCreateSemaphore max %u current %u", max, current);

    // allocate it
    auto sem = new naive_sem_t;
    if(!sem) {
        return AE_NO_MEMORY;
    }

    // initialize it
    sem->val = current;
    sem->max = max;

    // done!
    *outHandle = sem;
    return AE_OK;
}

/**
 * Deletes a previously allocated semaphore.
 */
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle) {
    Trace("AcpiOsDeleteSemaphore %p", handle);

    delete handle;
    return AE_OK;
}

/**
 * Signals a semaphore.
 */
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE sem, UINT32 units) {
    Warn("%s unimplemented", __PRETTY_FUNCTION__);
    return AE_NOT_IMPLEMENTED;
}

/**
 * Waits for a semaphore to be signalled.
 *
 * @param timeout How long, in ms, to wait. A value of 0xFFFF indicates forever.
 */
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE sem, UINT32 units, UINT16 timeout) {
    Warn("%s unimplemented", __PRETTY_FUNCTION__);
    return AE_NOT_IMPLEMENTED;
}
