#include <stddef.h>
#include <log.h>

extern "C" void __cxa_pure_virtual();

/**
 * Virtual function stub
 */
extern "C" void __cxa_pure_virtual() {
    panic("pure virtual function called");
}

/// Allocates memory
void *operator new(size_t size) {
    // TODO: implement
    return (void *) 0xDEADBEEF;
}
/// Allocates an array
void *operator new[](size_t size) {
    // TODO: implement
    return (void *) 0xDEADBEEF;
}
/// release previously allocated memory
void operator delete(void *p) {
    // TODO: implement
}
/// release previously allocated array memory
void operator delete[](void *p) {
    // TODO: implement
}
