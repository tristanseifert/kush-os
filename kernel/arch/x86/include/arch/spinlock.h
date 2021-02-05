#ifndef ARCH_X86_SPINLOCK_H
#define ARCH_X86_SPINLOCK_H

#include "private.h"
#include "atomic.h"

#include <stdint.h>

/**
 * Opaque spinlock data structure; you shouldn't grab any members out of this.
 */
typedef struct x86_spinlock {
    // actual lock flag
    volatile uint32_t count = 0;
    // reserved memory
    uint8_t reserved[60];
} __attribute__((aligned(64))) x86_spinlock_t;

/// Spins waiting for a lock to become available
static inline void x86_spin_lock(x86_spinlock_t *lock) {
    while(1) {
        if(!xchg_32(&lock->count, 1)) {
            // lock was successfully taken
            return;
        }
        // wait for spinlock to become free
        while(lock->count) {
            // provide microarchitectural hint that this is a spinlock
            asm volatile("pause\n": : :"memory");
        }
    }
}

// Unlocks the given spinlock
static inline void x86_spin_unlock(x86_spinlock_t *lock) {
    barrier();
    lock->count = 0;
}

/// attempts to lock the spinlock, bailing if it's not available
static inline int x86_spin_trylock(x86_spinlock_t *lock) {
    return xchg_32(&lock->count, 1);
}

namespace arch { namespace x86 {
class SpinLockGuard {
    public:
        /// allocate a new spin lock guard and take the lock
        SpinLockGuard(x86_spinlock_t *_lock) : lock(_lock) {
            x86_spin_lock(this->lock);
        }
        /// on destruction, release the lock
        ~SpinLockGuard() {
            x86_spin_unlock(this->lock);
        }

    private:
        x86_spinlock_t *lock = nullptr;
};
}}


#define SPIN_INIT { .count = 0 }
#define DECLARE_SPINLOCK(name) x86_spinlock_t name = SPIN_INIT
#define DECLARE_SPINLOCK_S(name) static x86_spinlock_t name = SPIN_INIT

#define SPIN_LOCK(name) x86_spin_lock(&name)
#define SPIN_TRY_LOCK(name) x86_spin_trylock(&name)
#define SPIN_UNLOCK(name) x86_spin_unlock(&name)
#define SPIN_LOCK_GUARD(name) arch::x86::SpinLockGuard UNIQUE_NAME(_x86_spinlock_guard)(&name);

#endif
