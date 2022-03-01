#include <Logging/Console.h>

extern "C" void __cxa_pure_virtual() {
    PANIC("%s", __FUNCTION__);
}
