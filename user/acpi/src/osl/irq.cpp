/*
 * Implementations of the ACPICA OS layer interrupt handlers.
 */
#include "osl.h"
#include "log.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <unordered_map>
#include <thread>
#include <system_error>
#include <sys/syscalls.h>
#include <acpi.h>

/// Information on an IRQ registration
struct IrqRegistration {
    /// associated notification bit
    uintptr_t bit = 0;

    /// IRQ Number
    uint32_t irq;
    /// irq handler handle
    uintptr_t handle;

    /// ACPICA handler routien
    ACPI_OSD_HANDLER handler;
    /// context for ACPICA handler
    void *context = nullptr;
};

/// Notification flag indicating an internal dispatcher event occurred.
constexpr static const uintptr_t kDispatcherEvent = 0x80000000;

/// As long as this is set, the IRQ dispatcher will continue processing interrupts
static std::atomic_bool gRun;
/// IRQ dispatching thread
static std::unique_ptr<std::thread> gDispatcher;
/// Handle for the dispatcher thread
static uintptr_t gDispatcherHandle;
/// Bits we've allocated to interrupts
static uintptr_t gAllocatedBits = 0;

/// IRQ registration lock
static std::mutex gRegistrationsLock;
/// Active interrupt registrations
static std::unordered_map<uint32_t, IrqRegistration> gRegistrations;


/**
 * Installs an interrupt handler.
 */
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER ServiceRoutine, void *ctx) {
    Trace("AcpiOsInstallInterruptHandler: irq %u, routine %p ctx %p", irq, (void *) ServiceRoutine, ctx);

    int err;
    uintptr_t bit = 0;
    bool foundBit = false;

    // allocate a notification bit
    for(size_t i = 0; i < 32; i++) {
        if(!(gAllocatedBits & (1 << i))) {
            bit = i;
            foundBit = true;
            break;
        }
    }

    if(!foundBit) {
        Warn("No available irq bits");
        return AE_ERROR;
    }

    // mark as allocated
    gAllocatedBits |= (1 << bit);

    // build up info structure
    IrqRegistration reg;
    reg.bit = bit;
    reg.irq = irq;
    reg.handler = ServiceRoutine;
    reg.context = ctx;

    // register the IRQ handler
    err = IrqHandlerInstall(irq, gDispatcherHandle, (1 << bit), &reg.handle);
    if(err) {
        Warn("IrqHandlerInstall() failed: %d", err);
        gAllocatedBits &= ~(1 << bit);
        return AE_ERROR;
    }

    Trace("irq %u -> bit %u", irq, bit);

    std::lock_guard<std::mutex> lg(gRegistrationsLock);
    gRegistrations.emplace(irq, std::move(reg));

    // the handler is now active
    return AE_OK;
}

/**
 * Removes a previously installed interrupt handler.
 */
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER ServiceRoutine) {
    Trace("AcpiOsRemoveInterruptHandler: irq %u, routine %p", irq, (void *) ServiceRoutine);

    if(!ServiceRoutine) {
        return AE_BAD_PARAMETER;
    }

    // handler for the irq
    if(!gRegistrations.contains(irq)) {
        return AE_NOT_EXIST;
    }

    // remove it
    if(gRegistrations.at(irq).handler != ServiceRoutine) {
        return AE_BAD_PARAMETER;
    }

    gRegistrations.erase(irq);

    return AE_OK;
}



/**
 * Main loop of the dispatch thread
 */
static void DispatcherMain() {
    int err;
    uintptr_t note;

    // get some info
    ThreadSetName(0, "irq dispatch");
    err = ThreadGetHandle(&gDispatcherHandle);
    if(err) {
        throw std::system_error(err, std::system_category(), "ThreadGetHandle");
    }

    // set a high priority
    err = ThreadSetPriority(gDispatcherHandle, 80);
    if(err) {
        throw std::system_error(err, std::system_category(), "ThreadSetPriority");
    }

    Success("IRQ dispatcher ready");

    // process requests
    while(gRun) {
        // receive all notifications and handle our notify bits
        note = NotificationReceive(UINTPTR_MAX);
        Trace("Notify %08x", note);

        if(note & kDispatcherEvent) {
            // nothing to handle
        }

        // handle actual interrupts
        const auto irqBits = (note & ~kDispatcherEvent);

        if(irqBits) {
            bool handled = false;
            //std::lock_guard<std::mutex> lg(gRegistrationsLock);

            for(const auto &[irq, handler] : gRegistrations) {
                // is this the correct handler? if so, invoke it
                if(note & (1 << handler.bit)) {
                    handler.handler(handler.context);
                    handled = true;
                }
            }

            if(!handled) {
                Warn("Unhandled irq notify %08x", irqBits);
            }
        }
    }

    // clean up
    Info("IRQ dispatcher exiting");
}

/**
 * Initializes the ACPI interrupt dispatcher thread.
 */
void osl::InitIrqDispatcher() {
    // ensure we don't allocate the dispatcher event bit
    gAllocatedBits |= kDispatcherEvent;

    // start the thread
    gRun = true;
    gDispatcher = std::make_unique<std::thread>(&DispatcherMain);
}

/**
 * Stops the IRQ dispatcher thread.
 */
void osl::StopIrqDispatcher() {
    int err;

    // request stop and send a dummy notification
    gRun = false;

    err = NotificationSend(gDispatcherHandle, kDispatcherEvent);
    if(err) {
        Warn("failed to send irq dispatcher stop notification: %d", err);
    }

    // wait for dispatcher to exit
    gDispatcher->join();
    gDispatcher = nullptr;
}
