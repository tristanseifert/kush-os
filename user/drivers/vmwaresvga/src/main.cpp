#include <memory>

#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>
#include <libpci/UserClient.h>

#include "Log.h"
#include "SVGA.h"

const char *gLogTag = "svga";

/**
 * Entry point for the SVGA driver; the provided arguments are the forest paths of the PCI devices
 * the device is attached to.
 */
int main(const int argc, const char **argv) {
    int err;
    if(argc != 2) {
        Abort("You must specify the forest path of a device.");
    }

    // disable the kernel console first
    err = Amd64SetKernelFbConsEnabled(false);
    if(err) {
        Abort("Failed to disable kernel framebuffer console: %d", err);
    }

    // get the PCI device and create the SVGA device from it
    std::shared_ptr<libpci::Device> pciDev;
    err = libpci::Device::Alloc(argv[1], pciDev);
    if(err) {
        Abort("Failed to allocate PCIe device for '%s': %d", argv[1], err);
    }

    std::shared_ptr<SVGA> dev;
    err = SVGA::Alloc(pciDev, dev);
    if(err) {
        Abort("Failed to initialize device at '%s': %d", pciDev->getPath().c_str(), err);
    }

    Trace("SVGA device: %p (%d)", dev.get(), err);

    // TODO: enter message loop
    while(1) {
        ThreadUsleep(1000 * 1000);
    }
}
