#ifndef _LIBC_THREADS_H
#define _LIBC_THREADS_H

#include <_libc.h>

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef timespec
struct timespec;
#endif

// forward declare all the types
typedef struct uthread *thrd_t;
typedef struct ucondvar *cnd_t;
typedef struct umutex *mtx_t;
typedef uintptr_t tls_key;

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

#ifdef __cplusplus
}
#endif
#endif
