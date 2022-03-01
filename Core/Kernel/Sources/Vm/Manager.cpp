#include "Vm/Manager.h"
#include "Vm/Map.h"
#include "Vm/MapEntry.h"

#include "Exceptions/Handler.h"
#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>
#include <new>

using namespace Kernel::Vm;

Manager *Manager::gShared{nullptr};
// space in .bss segment for the VM manager
static KUSH_ALIGNED(64) uint8_t gVmmBuf[sizeof(Manager)];

/**
 * @brief Initialize the global VM manager instance.
 */
void Manager::Init() {
    REQUIRE(!gShared, "cannot re-initialize VM manager");

    auto ptr = reinterpret_cast<Manager *>(&gVmmBuf);
    new (ptr) Manager();

    gShared = ptr;
}

/**
 * @brief Handles page faults.
 *
 * This looks up the corresponding VM object (if any) in the current address space for the
 * faulting address.
 *
 * @param state Processor state at the time of the fault
 * @param faultAddr Faulting virtual address
 */
void Manager::HandleFault(Platform::ProcessorState &state, const uintptr_t faultAddr) {
    // TODO: find the VM object and invoke its fault handler

    // if the fault was caused by kernel code and is yet unhandled, abort
    if(state.getPc() >= Platform::KernelAddressLayout::KernelBoundary) {
        Exceptions::Handler::AbortWithException(Exceptions::Handler::ExceptionType::PageFault,
                state, reinterpret_cast<void *>(faultAddr));
    }
}

