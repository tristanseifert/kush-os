/*
 * Implements reader/writer locks. This allows multiple threads to concurrently read a data
 * structure, but only one writer (with no pending readers.)
 */
#ifndef ARCH_AMD64_RWLOCK_H
#define ARCH_AMD64_RWLOCK_H

#include "atomic.h"
#include "private.h"

#include <stdint.h>

/**
 * Opaque rw lock data structure
 */
typedef union amd64_rwlock {
    unsigned u;
    unsigned short us;
    __extension__ struct {
        uint8_t write;
        uint8_t read;
        uint8_t users;
    } s;
} __attribute__((aligned(64))) amd64_rwlock_t;



/**
 * Spins until we can acquire the rwlock as a writer.
 */
static inline void amd64_rwlock_wrlock(amd64_rwlock_t *l) {
    unsigned me = atomic_xadd(&l->u, (1<<16));
    uint8_t val = me >> 16;

    while (val != l->s.write) asm volatile("pause\n": : :"memory");
}

/**
 * Release a writer lock on an rwlock.
 */
static inline void amd64_rwlock_wrunlock(amd64_rwlock_t *l) {
    amd64_rwlock_t t = *l;
    barrier();

    t.s.write++;
    t.s.read++;

    *(unsigned short *) l = t.us;
}

/**
 * Attempt to take an rwlock as a writer.
 *
 * @return 0 if lock was successfully taken, 1 otherwise.
 */
static inline int amd64_rwlock_wrtrylock(amd64_rwlock_t *l) {
    unsigned me = l->s.users;
    uint8_t menew = me + 1;
    unsigned read = l->s.read << 8;
    unsigned cmp = (me << 16) + read + me;
    unsigned cmpnew = (menew << 16) + read + me;

    if(cmpxchg(&l->u, cmp, cmpnew) == cmp) return 0;

    return 1;
}

/**
 * Spins until we can acquire the rwlock as a reader.
 */
static inline void amd64_rwlock_rdlock(amd64_rwlock_t *l) {
    unsigned me = atomic_xadd(&l->u, (1<<16));
    uint8_t val = me >> 16;

    while(val != l->s.read) asm volatile("pause\n": : :"memory");
    l->s.read++;
}

/**
 * Unlocks the reader lock.
 */
static inline void amd64_rwlock_rdunlock(amd64_rwlock_t *l) {
    atomic_inc(&l->s.write);
}

/**
 * Attempt to take an rwlock as a reader.
 *
 * @return 0 if lock was successfully taken, 1 otherwise.
 */
static inline int amd64_rwlock_rdtrylock(amd64_rwlock_t *l) {
    unsigned me = l->s.users;
    unsigned write = l->s.write;
    uint8_t menew = me + 1;
    unsigned cmp = (me << 16) + (me << 8) + write;
    unsigned cmpnew = ((unsigned) menew << 16) + (menew << 8) + write;

    if(cmpxchg(&l->u, cmp, cmpnew) == cmp) return 0;

    return 1;
}

namespace arch { namespace amd64 {
/// Wraps an RW lock in a RAII-style guard
class RwLockGuard {
    public:
        /// allocate a new rw lock guard and take the lock
        RwLockGuard(amd64_rwlock_t *_lock, const bool _writer) : lock(_lock), writer(_writer) {
            if(_writer) {
                amd64_rwlock_wrlock(_lock);
            } else {
                amd64_rwlock_rdlock(_lock);
            }
        }
        /// on destruction, release the lock
        ~RwLockGuard() {
            if(this->writer) {
                amd64_rwlock_wrunlock(this->lock);
            } else {
                amd64_rwlock_rdunlock(this->lock);
            }
        }

    private:
        amd64_rwlock_t *lock = nullptr;
        bool writer = false;
};
}}


#define RWLOCK_INIT { .u = 0 }
#define DECLARE_RWLOCK(name) amd64_rwlock_t name = RWLOCK_INIT
#define DECLARE_RWLOCK_S(name) static amd64_rwlock_t name = RWLOCK_INIT

#define RW_LOCK_READ(name) amd64_rwlock_rdlock(name)
#define RW_TRY_LOCK_READ(name) amd64_rwlock_rdtrylock(name)
#define RW_UNLOCK_READ(name) amd64_rwlock_rdunlock(name)
#define RW_LOCK_READ_GUARD(name) arch::amd64::RwLockGuard UNIQUE_NAME(_amd64_rwlock_guard)(&name, false);

#define RW_LOCK_WRITE(name) amd64_rwlock_wrlock(name)
#define RW_TRY_LOCK_WRITE(name) amd64_rwlock_wrtrylock(name)
#define RW_UNLOCK_WRITE(name) amd64_rwlock_wrunlock(name)
#define RW_LOCK_WRITE_GUARD(name) arch::amd64::RwLockGuard UNIQUE_NAME(_amd64_rwlock_guard)(&name, true);

#endif
