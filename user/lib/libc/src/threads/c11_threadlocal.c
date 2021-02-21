#include <threads.h>

#include <stdlib.h>
#include <stdio.h>

#include "struct/hashmap.h"

#include <sys/syscalls.h>
#include <sys/queue.h>

/**
 * Info for a registered thread local slot.
 */
struct tls_slot_info {
    /// slot key
    uintptr_t key;
    /// destructor, if any
    tss_dtor_t _Nullable dtor;
};

/**
 * Returns a hash code for a TLS slot info struct; it computes the hash of the key.
 */
static uint64_t _HashmapTlsSlotHash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct tls_slot_info *slot = (const struct tls_slot_info *) item;
    return hashmap_sip(&slot->key, sizeof(slot->key), seed0, seed1);
}
/**
 * Compares two threads by their handles.
 */
static int _HashmapTlsSlotCompare(const void *a, const void *b, void *udata) {
    const struct tls_slot_info *ta = (const struct tls_slot_info *) a;
    const struct tls_slot_info *tb = (const struct tls_slot_info *) b;

    return ta->key < tb->key;
}

/**
 * Global state for mapping thread local keys to destructors and offsets and such
 */
static struct {
    /// mutex protecting access to all our structures
    mtx_t lock;

    /// a hashmap containing the tss key -> info mapping
    struct hashmap *entries;
    /// next TSS key to allocate
    tss_t nextKey;
} gState;

/**
 * Initializes the thread-local storage list.
 */
void __libc_tss_init() {
    int err;

    // set up the lock
    err = mtx_init(&gState.lock, mtx_plain);
    if(err != thrd_success) abort();

    gState.nextKey = 1;

    // and now, the hashmap
    gState.entries = hashmap_new(sizeof(struct tls_slot_info), 0, 'TLS ', 'SLOT', 
            _HashmapTlsSlotHash, _HashmapTlsSlotCompare, NULL);
    if(!gState.entries) abort();
}



/**
 * Allocates a new thread-local storage slot.
 */
int tss_create(tss_t * _Nonnull outKey, tss_dtor_t _Nullable destructor) {
    int err;

    // allocate the index
    const uintptr_t key = __atomic_fetch_add(&gState.nextKey, 1, __ATOMIC_RELAXED);
    *outKey = key;

    // build the info struct
    struct tls_slot_info info;
    info.key = key;
    info.dtor = destructor;

    // insert it
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) {
        return thrd_error;
    }

    void *ret = hashmap_set(gState.entries, &info);
    if(ret || hashmap_oom(gState.entries)) {
        // either an item was replaced, or we ran out of memory
        goto fail;
    }

    // done :D
    mtx_unlock(&gState.lock);
    return thrd_success;

    // handle error conditions: ensure the lock is unlocked again
fail:;
    mtx_unlock(&gState.lock);
    return thrd_error;
}

/**
 * Returns the value of a thread local storage slot.
 */
void *tss_get(tss_t key) {
    fprintf(stderr, "tss_get() unimplemented");
    return NULL;
}

/**
 * Sets the value of a thread local storage slot.
 */
int tss_set(tss_t key, void *value) {
    int err;
    struct tls_slot_info info, *item;
    info.key = key;

    // acquire lock over the TLS data structures
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) {
        return thrd_error;
    }

    // look up the thread local index for it and store it
    void *_item = hashmap_get(gState.entries, &info);
    if(!_item) {
        goto fail;
    }
    item = (struct tls_slot_info *) _item;

    // TODO: store item
    return thrd_success;

    // handle error conditions
fail:;
    mtx_unlock(&gState.lock);
    return thrd_error;
}
