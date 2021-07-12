#include "SVGA.h"
#include "Commands2D.h"
#include "FIFO.h"
#include "RpcServer.h"
#include "Log.h"

#include <cstdint>
#include <cstdlib>
#include <unistd.h>

#include <svga_reg.h>
#include <svga3d_reg.h>

#include <libpci/Device.h>
#include <driver/DrivermanClient.h>
#include <DriverSupport/gfx/Display.h>
#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>

/// Region of virtual memory in which the SVGA device maps stuff
static uintptr_t kPrivateMappingRange[2] = {
    // start
    0x11000000000,
    // end
    0x11080000000,
};



/**
 * Attempt to allocate an SVGA device driver object for the given PCI device.
 */
int SVGA::Alloc(const std::shared_ptr<libpci::Device> &dev, std::shared_ptr<SVGA> &out) {
    std::shared_ptr<SVGA> ptr(new SVGA(dev));
    if(!ptr->status) {
        out = ptr;
    }
    return ptr->status;
}



/**
 * Initializes an SVGA device for the given PCI device. This will ensure the provided device is
 * actually something we can control and maps its buffers and performs some capability discovery to
 * figure out what the device supports.
 */
SVGA::SVGA(const std::shared_ptr<libpci::Device> &_device) : device(_device) {
    using BA = libpci::Device::BaseAddress;

    Trace("Setting up SVGA device $%p for %s", this, _device->getPath().c_str());

    /*
     * Find the IO register BAR and initialize it. Then, we'll negotiate the device version to use
     * before mapping the remaining memory regions.
     */
    const auto &bars = _device->getAddressResources();
    for(const auto &a : bars) {
        switch(a.bar) {
            /*
             * IO register range; this is used for some immediate (with side effects) accesses to
             * the graphics hardware. It should be an IO space region.
             *
             * We just add the provided range to our acess allowed list and go on.
             */
            case BA::BAR0:
                if(!this->mapRegisters(a)) {
                    Warn("Failed to map %s", "IO registers");
                }
                break;

            default:
                break;
        }
    }

    if(!this->ioBase) {
        this->status = Errors::MissingBar;
        return;
    }

    /*
     * Determine the device version supported by the VM hypervisor. Once we've figured this out,
     * read some buffer sizes and device capabilities, and enable interrupts if supported.
     */
    this->version = SVGA_ID_2;
    do {
        this->regWrite(SVGA_REG_ID, this->version);
        if(this->regRead(SVGA_REG_ID) == this->version) {
            break;
        } else {
            this->version--;
        }
    } while(this->version >= SVGA_ID_2);

    if(this->version < SVGA_ID_0) {
        Warn("Failed to negotiate SVGA version");
        this->status = Errors::UnsupportedVersion; return;
    }

    this->vramSize = this->regRead(SVGA_REG_VRAM_SIZE);
    this->vramFramebufferSize = this->regRead(SVGA_REG_FB_SIZE);

    REQUIRE(this->vramFramebufferSize >= 0x100000, "Framebuffer reservation too small: $%x",
            this->vramFramebufferSize);

    this->caps = this->regRead(SVGA_REG_CAPABILITIES);
    if(kLogInit) Trace("Capabilities: $%08x", this->caps);

    if(!this->initIrq()) {
        Warn("Failed to initialize IRQs");
        return;
    }

    /*
     * Now process the remaining BARs to initialize the memory regions. Deferring their
     * initialization until now means they can access registers freely.
     */
    for(const auto &a : bars) {
        switch(a.bar) {
            /**
             * Guest framebuffer range; this is a memory range that's mapped as write combining and
             * cacheable.
             */
            case BA::BAR1:
                if(!this->mapVram(a)) {
                    Warn("Failed to map %s", "VRAM");
                }
                break;

            /// Create the FIFO handler.
            case BA::BAR2:
                this->fifo = std::make_unique<svga::FIFO>(this, a);
                break;

            default:
                break;
        }
    }

    if(!this->fifo || this->fifo->getStatus()) {
        this->status = this->fifo ? this->fifo->getStatus() : Errors::MissingBar;
        return;
    }

    /*
     * The device has been fully initialized. We can now create the RPC service, the various
     * command handlers for, set the initial mode and enable video output.
     */
    uintptr_t port;
    this->status = PortCreate(&port);
    if(this->status) {
        Warn("%s failed: %d", "PortCreate", this->status);
        return;
    }

    this->rpc = std::make_unique<svga::RpcServer>(this, port);
    this->registerUnder(this->device->getPath());

    this->cmd2d = std::make_unique<svga::Commands2D>(this);

    const auto [w, h, bpp] = kDefaultMode;
    this->setMode(w, h, bpp, false);

    this->enable();
}

/**
 * Initializes the IO region by adding the described IO region to the allow list.
 *
 * We expect this region is 16 bytes in length.
 */
bool SVGA::mapRegisters(const BAR &bar) {
    REQUIRE(bar.length >= 16, "Invalid IO BAR length: %lu", bar.length);

    static const uint8_t bitmap[4] {0xFF, 0xFF, 0xFF, 0xFF};
    this->status = Amd64UpdateAllowedIoPorts(bitmap, bar.length, bar.base);

    this->ioBase = bar.base;
    return !this->status;
}

/**
 * Creates a physical allocation region for the VRAM of the device. This region is mapped write
 * combining.
 */
bool SVGA::mapVram(const BAR &bar) {
    int err;
    uintptr_t base{0};

    // round up size to page multiple
    const auto pageSz = sysconf(_SC_PAGESIZE);
    const auto size = ((bar.length + pageSz - 1) / pageSz) * pageSz;

    // create the phys region
    err = AllocVirtualPhysRegion(bar.base, size, VM_REGION_RW | VM_REGION_WRITETHRU, &this->vramHandle);
    if(err) {
        Warn("%s failed: %d", "AllocVirtualPhysRegion", err);
        this->status = err; return false;
    }

    // map it
    err = MapVirtualRegionRange(this->vramHandle, kPrivateMappingRange, size, 0, &base);
    kPrivateMappingRange[0] += size + pageSz; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegion", err);
        this->status = err; return false;
    }

    this->vram = reinterpret_cast<void *>(base);
    this->vramSize = bar.length;
    return true;
}

/**
 * Initializes the interrupt handler for the device.
 */
bool SVGA::initIrq() {
    int err;

    // bail if IRQs aren't supported
    if(!(this->caps & SVGA_CAP_IRQMASK)) {
        if(kLogInit) Warn("SVGA device %s doesn't support interrupts",
                this->device->getPath().c_str());
        return true;
    }

    // mask all IRQs
    this->regWrite(SVGA_REG_IRQMASK, 0);
    // clear any pending IRQs
    err = Amd64PortWriteL(this->ioBase + SVGA_IRQSTATUS_PORT, 0, 0xFF);
    if(err) {
        Warn("%s failed: %d", "Amd64PortWriteL");
        this->status = err; return false;
    }

    // TODO: implement the remainder of this
    return false;
}

/**
 * Cleans up all resources we've allocated.
 */
SVGA::~SVGA() {
    this->disable();

    // remove the allocated regions
    if(this->vramHandle) {
        DeallocVirtualRegion(this->vramHandle);
    }
}



/**
 * Reads from a register in the SVGA device's IO space. Access is performed via the indexed access
 * port mechanism.
 */
uint32_t SVGA::regRead(const size_t reg) {
    int err;
    uint32_t temp{0};

    // write to the index port
    err = Amd64PortWriteL(this->ioBase + SVGA_INDEX_PORT, 0, static_cast<uint32_t>(reg));
    REQUIRE(!err, "%s failed: %d", "Amd64PortWriteL", err);

    // read from data port
    err = Amd64PortReadL(this->ioBase + SVGA_VALUE_PORT, 0, &temp);
    REQUIRE(!err, "%s failed: %d", "Amd64PortReadL", err);

    return temp;
}

/**
 * Writes to a register in the SVGA device's IO space.
 */
void SVGA::regWrite(const size_t reg, const uint32_t value) {
    int err;

    // write to the index port 
    err = Amd64PortWriteL(this->ioBase + SVGA_INDEX_PORT, 0, static_cast<uint32_t>(reg));
    REQUIRE(!err, "%s failed: %d", "Amd64PortWriteL", err);

    // write the data value
    err = Amd64PortWriteL(this->ioBase + SVGA_VALUE_PORT, 0, value);
    REQUIRE(!err, "%s failed: %d", "Amd64PortWriteL", err);
}


/**
 * Enables the SVGA device. This will configure the FIFO for command submission as well before
 * the video output is enabled.
 */
void SVGA::enable() {
    if(this->enabled) return;

    // enable the device and FIFO
    this->regWrite(SVGA_REG_ENABLE, true);
    this->regWrite(SVGA_REG_CONFIG_DONE, true);

    // set enabled flag
    this->enabled = true;

    // XXX: this is where we'd perform an irq test
    memset(this->vram, 0, this->vramFramebufferSize);

    int err = this->cmd2d->update();

    if(err) {
        Warn("Failed to update display: %d", err);
        this->status = err;
    }
}

/**
 * Disables the SVGA device's video output.
 */
void SVGA::disable() {
    if(!this->enabled) return;

    // disable the adapter
    this->regWrite(SVGA_REG_ENABLE, false);

    // clear enabled flag
    this->enabled = false;
}



/**
 * Sets the video mode of the adapter. If it's not already enabled, it will be enabled as a result
 * of the call.
 *
 * @param width Screen width, in pixels
 * @param height Screen height, in pixels
 * @param bpp Bits per pixel; only 8/24/32 are supported.
 * @param enable When set, the device is enabled before we return
 *
 * @return 0 on success, an error code otherwise.
 */
int SVGA::setMode(const uint32_t width, const uint32_t height, const uint8_t bpp,
        const bool enable) {
    // validate arguments
    if(width % 8 || height % 8 || (bpp != 8 && bpp != 24 && bpp != 32)) {
        return Errors::InvalidMode;
    }

    // disable video output if needed
    if(this->enabled) {
        this->regWrite(SVGA_REG_ENABLE, false);
    }

    // write registers
    this->regWrite(SVGA_REG_WIDTH, width);
    this->regWrite(SVGA_REG_HEIGHT, height);
    this->regWrite(SVGA_REG_BITS_PER_PIXEL, bpp);

    // enable again
    if(enable) {
        this->regWrite(SVGA_REG_ENABLE, true);
        this->enabled = true;
    }

    // update bookkeeping
    this->fbSize = {this->regRead(SVGA_REG_WIDTH), this->regRead(SVGA_REG_HEIGHT)};
    this->fbBpp = this->regRead(SVGA_REG_BITS_PER_PIXEL);
    this->fbPitch = this->regRead(SVGA_REG_BYTES_PER_LINE);

    if(kLogModeset) Trace("New mode: %lu x %lu, %u bpp pitch %lu", this->fbSize.first,
            this->fbSize.second, this->fbBpp, this->fbPitch);

    return 0;
}



/**
 * Registers the device in the driver forest as a leaf of the PCI device that we attached to, one
 * for each display.
 *
 * TODO: support multiple displays
 */
void SVGA::registerUnder(const std::string_view &parentDevice) {
    auto dm = libdriver::RpcClient::the();

    // serialize connection info
    std::vector<std::byte> info;
    this->rpc->encodeInfo(info);

    // try to register the device
    this->forestPath = dm->AddDevice(std::string(parentDevice), std::string(kDeviceName));
    REQUIRE(!this->forestPath.empty(), "Failed to register device in forest (under %s)",
            parentDevice.data());

    if(kLogInit) Trace("Registered device at %s", this->forestPath.c_str());

    // set the connection info
    dm->SetDeviceProperty(this->forestPath, DriverSupport::gfx::Display::kConnectionPropertyName, info);

    // and start the device
    int err = dm->StartDevice(this->forestPath);
    REQUIRE(!err, "Failed to start device: %d", err);
}

/**
 * Enters the main run loop for the driver.
 *
 * Currently, this just enters the RPC server's main loop.
 */
int SVGA::runLoop() {
    return this->rpc->run();
}
