#include "Mapper.h"

#include <log.h>

#include <new>

using namespace vm;

static char gMapperBuf[sizeof(Mapper)];
Mapper *Mapper::gShared = (Mapper *) &gMapperBuf;

/**
 * Runs the constructor for the statically allocated mapper struct.
 */
void Mapper::init() {
    new(gShared) Mapper();
}

/**
 * Initializes the VM mapper. This sets up the kernel's memory mapping, and maps the existing
 * kernel data (text section, as well as rw data) into the map.
 */
Mapper::Mapper() {
    // placement allocate the kernel map
}



/**
 * Loads a given map into the CPU.
 *
 * @note It's the caller's responsibility to invalidate any addresses that may remain in the MMU
 * translation cache.
 */
void Mapper::loadMap(Map *map) {

}

/**
 * Convenience helper to load the kernel map into the processor.
 */
void Mapper::loadKernelMap() {
    // loadMap(gKernelMap);
}
