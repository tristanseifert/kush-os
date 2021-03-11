#include "Manager.h"

#include <new>
#include <stdint.h>

#include <log.h>

using namespace platform;

static uint8_t gSharedBuf[sizeof(IrqManager)] __attribute__((aligned(alignof(IrqManager))));
IrqManager *IrqManager::gShared = nullptr;

/**
 * Initializes the shared irq manager.
 */
void IrqManager::Init() {
    REQUIRE(!gShared, "cannot re-initialize irq manager");

    gShared = reinterpret_cast<IrqManager *>(&gSharedBuf);
    new (gSharedBuf) IrqManager;
}
