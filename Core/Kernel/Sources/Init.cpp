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
    Console::Notice("Welcome to the ~ chill zone ~");

    // TODO: do stuff
}
