#include "map.h"
#include "struct/hashmap.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <threads.h>

/**
 * Global state for the file descriptor mapping
 */
static struct {
    /// number to assign to the next file descriptor
    int nextFd;
    /// lock to protect concurrent access to the hashmap
    mtx_t lock;
    /// Actual hashmap containing file descriptors
    struct hashmap *entries;
} gState;

/**
 * A single entry in the file descriptor map
 */
typedef struct fd_map_entry {
    /// file descriptor number
    int key;
    /// pointer to file stream
    stream_t *stream;
} fd_map_entry_t;


/**
 * Returns a hash code for a TSS info struct; it computes the hash of the key.
 */
static uint64_t _HashmapSlotHash(const void *item, uint64_t seed0, uint64_t seed1) {
    const fd_map_entry_t *slot = (const fd_map_entry_t *) item;
    return hashmap_sip(&slot->key, sizeof(slot->key), seed0, seed1);
}
/**
 * Compares two threads by their handles.
 */
static int _HashmapSlotCompare(const void *a, const void *b, void *udata) {
    const fd_map_entry_t *ta = (const fd_map_entry_t *) a;
    const fd_map_entry_t *tb = (const fd_map_entry_t *) b;

    return ta->key < tb->key;
}

/**
 * Initialize the file descriptor -> IO stream map. The hashmap that stores these associations is
 * initialized.
 */
void InitFdToStreamMap() {
    int err;

    // start assigning after the standard IO streams
    gState.nextFd = STDERR_FILENO + 1;

    // set up the lock
    err = mtx_init(&gState.lock, mtx_plain);
    if(err != thrd_success) abort();

    // and now, the hashmap
    gState.entries = hashmap_new(sizeof(fd_map_entry_t), 0, 'FDES', 'YEET',
            _HashmapSlotHash, _HashmapSlotCompare, NULL);
    if(!gState.entries) abort();
}

/**
 * Registers a stream with the file descriptor map. A fake file descriptor number is allocated for
 * the stream and stored in the structure.
 *
 * @return 0 on success, error code otherwise.
 */
int RegisterFdStream(stream_t *stream, bool allocateFd) {
    int err;

    // allocate a file descriptor number
    if(allocateFd) {
        int num = __atomic_add_fetch(&gState.nextFd, 1, __ATOMIC_RELAXED);
        if(num <= 0) {
            fprintf(stderr, "fd number overflow!\n");
            abort();
        }

        stream->fd = num;
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) abort();

    // register into the map
    fd_map_entry_t info = {
        .key    = stream->fd,
        .stream = stream
    };

    void *ret = hashmap_set(gState.entries, &info);
    if(ret || hashmap_oom(gState.entries)) {
        // either an item was replaced, or we ran out of memory
        err = -1;
        goto beach;
    }

    // we've successfully inserted the item
    err = 0;

beach:;
    // and unlock
    mtx_unlock(&gState.lock);
    return err;
}


/**
 * Unregisters a previously registered file descriptor number.
 *
 * @return 0 if the stream was successfully removed, -1 if it was never registered.
 */
int UnregisterFdStream(stream_t *stream) {
    int err;

    // validate the file descriptor number
    if(stream->fd < 0) return -1;

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) abort();

    // remove it
    fd_map_entry_t info = {
        .key    = stream->fd,
        .stream = NULL
    };

    void *original = hashmap_delete(gState.entries, &info);

    // unlock
    mtx_unlock(&gState.lock);
    return (!original ? 0 : -1);
}

/**
 * Performs a lookup from file descriptor number to file stream struct.
 */
stream_t *ConvertFdToStream(int fd) {
    int err;

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) abort();

    // get it
    fd_map_entry_t info = {
        .key    = fd,
        .stream = NULL
    };

    fd_map_entry_t *found = hashmap_get(gState.entries, &info);

    // unlock and return the file stream
    mtx_unlock(&gState.lock);
    return (found ? found->stream : NULL);
}
