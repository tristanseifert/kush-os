/**
 * Entry point, called from the bootloader.
 *
 * At this point, we have the following guarantees about the environment:
 *
 * - Stack is properly configured
 * - Virtual address mapped as requested in ELF program headers
 * - All segments are 64-bit disabled.
 * - GDT is loaded with bootloader-provided GDT.
 * - No IDT is specified.
 * - NX bit enabled, paging enabled, A20 gate opened
 * - All PIC and IOAPIC IRQs masked
 * - UEFI boot services exited
 */
#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>

#include <Init.h>
#include <Logging/Console.h>

#include "Helpers.h"
#include "Arch/Gdt.h"
#include "Arch/Idt.h"
#include "Io/Console.h"
#include "Util/Backtrace.h"

using namespace Platform::Amd64Uefi;

/**
 * Entry point from the bootloader.
 */
extern "C" void _osentry(struct stivale2_struct *loaderInfo) {
    // set up the console (bootloader terminal, serial, etc.)
    Console::Init(loaderInfo);
    Kernel::Console::Init();

    // initialize CPU stuff
    Gdt::Init();
    Idt::InitBsp();

    // initialize some more stuff with bootloader info
    Backtrace::Init(loaderInfo);

    // jump to the kernel's entry point now
    Kernel::Start();

    PANIC("Kernel entry point returned!");
}
