/*
 * Initialization function for the kernel.
 */
#include <Init.h>
#include <Logging/Console.h>

using namespace Kernel;

/**
 * Kernel entry point
 *
 * This is where the kernel takes control from the platform-specific initialization code. The
 * machine state should be reasonably consistent, with a functional virtual memory map. We'll do
 * initialization here, roughly in two phases:
 *
 * 1. Initialize memory allocators, and bootstrap the virtual memory subsystem. The kernel will
 *    switch over to the newly generated memory map here.
 * 2. Set up the remainder of the system.
 *
 * Once initialization is complete, we attempt to load the initialization process from the boot
 * image, set up its main thread, and start the scheduler.
 */
void Kernel::Start() {
    Console::Notice("Welcome to \x1b[31mk\x1b[33mu\x1b[93ms\x1b[32mh\x1b[34m-\x1b[35mo\x1b[31ms\x1b[0m");

    // TODO: do stuff
}
