#ifndef _LIBC_THREADS_H
#define _LIBC_THREADS_H

#include <_libc.h>

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef timespec
struct timespec;
#endif

// userspace mutex
struct __umutex {
    // mutex flag
    uintptr_t flag;
    // when set, the lock is recursive
    char recursive;
    // thread that locked this mutex
    struct uthread * _Nullable thread;
    // ref count, if recursive
    int recursion;
};

// condition variable
struct __ucondvar {
    uintptr_t value;
};

// forward declare all the types
typedef struct uthread *thrd_t;
typedef struct __ucondvar cnd_t;
typedef struct __umutex mtx_t;
typedef uintptr_t tss_t;
typedef uintptr_t tls_key;
typedef uintptr_t once_flag;

#define thread_local _Thread_local

/* Function return values */
#define thrd_error    0 /**< The requested operation failed */
#define thrd_success  1 /**< The requested operation succeeded */
#define thrd_timedout 2 /**< The time specified in the call was reached without acquiring the requested resource */
#define thrd_busy     3 /**< The requested operation failed because a tesource requested by a test and return function is already in use */
#define thrd_nomem    4 /**< The requested operation failed because it was unable to allocate memory */

/** Thread start function.
* Any thread that is started with the @ref thrd_create() function must be
* started through a function of this type.
* @param arg The thread argument (the @c arg argument of the corresponding
*        @ref thrd_create() call).
* @return The thread return value, which can be obtained by another thread
* by using the @ref thrd_join() function.
*/
typedef int (*thrd_start_t)(void * _Nullable arg);

/** Create a new thread.
* @param thr Identifier of the newly created thread.
* @param func A function pointer to the function that will be executed in
*        the new thread.
* @param arg An argument to the thread function.
* @return @ref thrd_success on success, or @ref thrd_nomem if no memory could
* be allocated for the thread requested, or @ref thrd_error if the request
* could not be honored.
* @note A thread’s identifier may be reused for a different thread once the
* original thread has exited and either been detached or joined to another
* thread.
*/
LIBC_EXPORT int thrd_create(thrd_t _Nonnull * _Nonnull thr, thrd_start_t _Nonnull func, void * _Nullable arg);

/** Identify the calling thread.
* @return The identifier of the calling thread.
*/
LIBC_EXPORT thrd_t _Nonnull thrd_current(void);

/** Dispose of any resources allocated to the thread when that thread exits.
 * @return thrd_success, or thrd_error on error
*/
LIBC_EXPORT int thrd_detach(_Nonnull thrd_t thr);

/** Compare two thread identifiers.
* The function determines if two thread identifiers refer to the same thread.
* @return Zero if the two thread identifiers refer to different threads.
* Otherwise a nonzero value is returned.
*/
LIBC_EXPORT int thrd_equal(_Nonnull thrd_t thr0, _Nonnull thrd_t thr1);

/** Terminate execution of the calling thread.
* @param res Result code of the calling thread.
*/
LIBC_EXPORT __attribute__((noreturn)) void thrd_exit(int res);

/** Wait for a thread to terminate.
* The function joins the given thread with the current thread by blocking
* until the other thread has terminated.
* @param thr The thread to join with.
* @param res If this pointer is not NULL, the function will store the result
*        code of the given thread in the integer pointed to by @c res.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int thrd_join(_Nonnull thrd_t thr, int * _Nullable res);

/** Wait for a thread to terminate with the given timeout.
* The function joins the given thread with the current thread by blocking
* until the other thread has terminated.
* @param thr The thread to join with.
* @param res If this pointer is not NULL, the function will store the result
*        code of the given thread in the integer pointed to by @c res.
* @param timeout How long to wait for thread termination, or NULL to wait forever.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int thrd_join_np(_Nonnull thrd_t thr, int * _Nullable res,
        const struct timespec * _Nullable timeout);

/** Put the calling thread to sleep.
* Suspend execution of the calling thread.
* @param duration  Interval to sleep for
* @param remaining If non-NULL, this parameter will hold the remaining
*                  time until time_point upon return. This will
*                  typically be zero, but if the thread was woken up
*                  by a signal that is not ignored before duration was
*                  reached @c remaining will hold a positive time.
* @return 0 (zero) on successful sleep, -1 if an interrupt occurred,
*         or a negative value if the operation fails.
*/
LIBC_EXPORT int thrd_sleep(const struct timespec * _Nonnull duration,
        struct timespec * _Nullable remaining);

/** Yield execution to another thread.
* Permit other threads to run, even if the current thread would ordinarily
* continue to run.
*/
LIBC_EXPORT void thrd_yield(void);

/**
 * Sets the size of stacks for new threads.
 *
 * @param size Size of new thread stacks, in words.
 * @return thrd_success on success, or an error code
 */
LIBC_EXPORT int thrd_set_stacksize_np(const size_t size);

/**
 * Returns the native handle type for the given thread reference.
 */
LIBC_EXPORT uintptr_t thrd_get_handle_np(_Nonnull thrd_t thread);


// Mutex type flags
#define mtx_plain                       (1 << 0)
#define mtx_timed                       (1 << 1)
#define mtx_recursive                   (1 << 4)


/**
 * Creates a new mutex.
 *
 * The type can be one of the following:
 * - mtx_plain: Plain non-recursive mutex
 * - mtx_timed: Non-recursive mutex supporting timed waits
 * - mtx_plain | mtx_recursive: Creates a plain recursive mutex.
 * - mtx_timed | mtx_recursive: Creates a recursive mutex supporting timed waits.
 *
 * @return thrd_success if the mutex was created, thrd_error otherwise.
 */
LIBC_EXPORT int mtx_init(mtx_t * _Nonnull mutex, int type);

/**
 * Destroys a previously allocated mutex.
 */
LIBC_EXPORT void mtx_destroy(mtx_t * _Nonnull mutex);

/**
 * Blocks the current thread until the mutex is locked.
 *
 * @return thrd_success if successfully locked, thrd_error otherwise.
 */
LIBC_EXPORT int mtx_lock(mtx_t * _Nonnull mutex);

/**
 * Blocks the current thread until the given mutex is locked, or until the time point has elapsed.
 *
 * @return thrd_success if the mutex was locked, thrd_timedout if timeout expired, or thrd_error.
 */
LIBC_EXPORT int mtx_timedlock(mtx_t * _Nonnull mutex, const struct timespec * _Nonnull timePoint);

/**
 * Attempts to acquire the given mutex, but do not wait if needed.
 *
 * @return thrd_success if the mutex was locked, thrd_busy if it's already locked, or thrd_error
 * if an error occurred.
 */
LIBC_EXPORT int mtx_trylock(mtx_t * _Nonnull mutex);

/**
 * Unlocks a previously locked mutex.
 */
LIBC_EXPORT int mtx_unlock(mtx_t * _Nonnull mutex);



// Initializer for a once flag: it just has to be zero
#define ONCE_FLAG_INIT (0)

/**
 * Ensures the given function is called precisely once.
 */
LIBC_EXPORT void call_once(once_flag * _Nonnull flag, void (* _Nonnull func)(void));



/**
 * Destructor for thread-local storage.
 *
 * The argument passed to the destructor is the contents of the TSS slot.
 */
typedef void (*tss_dtor_t)(void * _Nullable contents);

/**
 * Allocates a new thread-local storage key.
 *
 * The key indices are shared across all threads, but the data stored at each index is unique for
 * each thread.
 *
 * Destructors for each slot are invoked up to TSS_DTOR_ITERATIONS times until their associated
 * slot is set to NULL.
 *
 * @param outKey Variable to receive the newly created key
 * @param destructor If not NULL, a destructor to invoke when the slot is removed
 * 
 * @return thrd_success if the slot was allocated, thrd_error otherwise.
 */
LIBC_EXPORT int tss_create(tss_t * _Nonnull outKey, tss_dtor_t _Nullable destructor);

/**
 * Returns the value of the given thread local storage key.
 *
 * On thread startup, all keys are initialized to NULL.
 *
 * @return Key value, or NULL on failure.
 */
LIBC_EXPORT void * _Nullable tss_get(tss_t key);

/**
 * Sets the value of a thread local storage key.
 *
 * It is safe to invoke this function from the TSS destructor.
 *
 * @return thrd_success if the key was set, thrd_error otherwise.
 */
LIBC_EXPORT int tss_set(tss_t key, void * _Nullable value);

/**
 * Deletes a previously allocated thread local storage key. You are responsible for ensuring all
 * threads have stopped using this key.
 *
 * @note Destructors will NOT be executed.
 *
 * @param key Key to deallocate
 */
LIBC_EXPORT void tss_delete(tss_t key);



/** Create a condition variable object.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int cnd_init(cnd_t * _Nonnull cond);

/** Release any resources used by the given condition variable.
* @param cond A condition variable object.
*/
LIBC_EXPORT void cnd_destroy(cnd_t * _Nonnull cond);

/** Signal a condition variable.
* Unblocks one of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int cnd_signal(cnd_t * _Nonnull cond);

/** Broadcast a condition variable.
* Unblocks all of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int cnd_broadcast(cnd_t * _Nonnull cond);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast. When the calling thread becomes unblocked it locks the mutex
* before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
LIBC_EXPORT int cnd_wait(cnd_t * _Nonnull cond, mtx_t * _Nonnull mtx);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast, or until after the specified time. When the calling thread
* becomes unblocked it locks the mutex before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @param xt A point in time at which the request will time out (absolute time).
* @return @ref thrd_success upon success, or @ref thrd_timeout if the time
* specified in the call was reached without acquiring the requested resource, or
* @ref thrd_error if the request could not be honored.
*/
LIBC_EXPORT int cnd_timedwait(cnd_t * _Nonnull cond, mtx_t * _Nonnull mtx, const struct timespec * _Nonnull ts);

#ifdef __cplusplus
}
#endif
#endif
