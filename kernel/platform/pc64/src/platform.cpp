#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "io/spew.h"
#include "irq/pic.h"
#include "timer/pit.h"

#include <bootboot.h>
#include <stdint.h>

using namespace platform;

/// bootloader info
extern "C" BOOTBOOT bootboot;
extern "C" uint8_t fb;

/**
 * Initializes the platform code.
 *
 * At this point, we disable a bunch of legacy stuff that may be enabled.
 */
void platform_init() {
    // configure debug printing
    Spew::Init();

    // sex
    int x, y, s=bootboot.fb_scanline, w=bootboot.fb_width, h=bootboot.fb_height;

    for(y=0;y<h;y++) { *((uint32_t*)(&fb + s*y + (w*2)))=0x00FFFFFF;  }
    for(x=0;x<w;x++) { *((uint32_t*)(&fb + s*(h/2)+x*4))=0x00FFFFFF;  }

    for(y=0;y<20;y++) { for(x=0;x<20;x++) { *((uint32_t*)(&fb + s*(y+20) + (x+20)*4))=0x00FF0000;  }  }
    for(y=0;y<20;y++) { for(x=0;x<20;x++) { *((uint32_t*)(&fb + s*(y+20) + (x+50)*4))=0x0000FF00;  }  }
    for(y=0;y<20;y++) { for(x=0;x<20;x++) { *((uint32_t*)(&fb + s*(y+20) + (x+80)*4))=0x000000FF;  }  }

    // parse phys mapping
    log("BootBoot info: %p (protocol %u, %u cores)", &bootboot, bootboot.protocol, bootboot.numcores);

    const auto &ar = bootboot.arch.x86_64;
    log("ACPI %p SMBI %p EFI %p MB %p", ar.acpi_ptr, ar.smbi_ptr, ar.efi_ptr, ar.mp_ptr);
    physmap_parse_bootboot(&bootboot);

    // set up and remap the PICs and other interrupt controllers
    irq::LegacyPic::disable();
    timer::LegacyPIT::disable();

    // detect some shit
    Physmap::DetectKernelPhys();
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {

}
