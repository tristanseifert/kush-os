#include "Manager.h"

#include <arch/critical.h>

#include "sched/Task.h"
#include "sched/Thread.h"

#include <new>
#include <log.h>

using namespace handle;

static uint8_t gSharedBuf[sizeof(Manager)] __attribute__((aligned(64)));
Manager *Manager::gShared = nullptr;

/**
 * Sets up the shared handle manager.
 */
void Manager::init() {
    gShared = reinterpret_cast<Manager *>(&gSharedBuf);
    new(gShared) Manager();
}

/**
 * Initializes the handle manager.
 */
Manager::Manager() {
    // does anything need to be done here?
}
