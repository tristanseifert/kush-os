#ifndef ACPISRV_OSL_OSL_H
#define ACPISRV_OSL_OSL_H

namespace osl {
/**
 * Initializes the interrupt dispatching mechanism.
 */
void InitIrqDispatcher();
/**
 * Initializes PCI configuration mechanism.
 */
void InitPciConfig();

/**
 * Tears down the interrupt dispatching mechanism.
 */
void StopIrqDispatcher();
}

#endif
