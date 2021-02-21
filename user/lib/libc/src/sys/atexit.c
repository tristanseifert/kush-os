#include <stdlib.h>
#include <stdio.h>

/**
 * Registers a function to be executed when the program is terminated, or the shared object is
 * unloaded from the caller's task.
 */
int __cxa_atexit(void (* _Nonnull func) (void * _Nullable), void * _Nullable arg,
        void * _Nullable dso_handle) {
    // TODO: implement
    fprintf(stderr, "Registering destructor %p, arg %p, DSO %p\n", func, arg, dso_handle);
    return 0;
}

/**
 * Invokes all destructors previously registered by calls to `__cxa_atexit` whose shared object
 * handle matches the specified value.
 *
 * @note After a destructor has been invoked, it's marked as used; this ensures they cannot be
 * executed more than once.
 *
 * @param handle Shared object handle for dstructors; if this is NULL, all destructors are
 * invoked.
 */
void __cxa_finalize (void * _Nullable d) {
    // TODO: implement
}

/**
 * Registers a function to be executed when the program is normally terminated, i.e. by a call to
 * the exit() function.
 *
 * Functions are registered in a stack; the last registered function is the first to be executed.
 */
int atexit(void (* _Nonnull func)(void)) {
    return __cxa_atexit((void(* _Nonnull)(void *)) func, NULL, NULL);
}
