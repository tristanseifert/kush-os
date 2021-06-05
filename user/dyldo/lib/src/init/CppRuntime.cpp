#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <new>

#include <malloc.h>

/**
 * Invoked if a pure virtual function is called.
 */
extern "C" void __cxa_pure_virtual() {
    abort();
}

/// Allocates memory
void *operator new(size_t size) {
    return malloc(size);
}
/// Aligned allocation
void *operator new(size_t size, std::align_val_t al) {
    void *result;
    int err = posix_memalign(&result, static_cast<size_t>(al), size);
    
    return (err ? nullptr : result);
}
/// Allocates an array
void *operator new[](size_t size) {
    return malloc(size);
}
/// release previously allocated memory
void operator delete(void *p) {
    free(p);
}
/// release previously allocated memory
void operator delete(void *p, size_t length) {
    // TODO: can we pass the length in for sanity checking?
    free(p);
}
/// release previously allocated array memory
void operator delete[](void *p) {
    free(p);
}
/// release aligned memory
void operator delete(void *p, std::align_val_t al) {
    free(p);
}

namespace std {
/// provide a bogus implementation of this getter; there are no exceptions here
int uncaught_exceptions() noexcept {
    return 0;
}
}

// we're single threaded, so stub out the C++ static allocation guards
extern "C" int __cxa_guard_acquire(uint64_t* guard_object) {
    return 0;
}
extern "C" void __cxa_guard_release(uint64_t *object) {
    *object |= 0xFF;
}

