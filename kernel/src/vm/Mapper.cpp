#include "Mapper.h"
#include "Map.h"

#include <arch.h>
#include <platform.h>
#include <log.h>

#include <new>

using namespace vm;

static char gMapperBuf[sizeof(Mapper)] __attribute__((aligned(64)));
Mapper *Mapper::gShared = nullptr;

static char gKernelMapBuf[sizeof(Map)] __attribute__((aligned(64)));
Map *Mapper::gKernelMap = nullptr;

/**
 * Runs the constructor for the statically allocated mapper struct.
 */
void Mapper::init() {
    gShared = reinterpret_cast<Mapper *>(&gMapperBuf);
    new(gShared) Mapper();
}

/**
 * Initializes the VM mapper. This sets up the kernel's memory mapping, and maps the existing
 * kernel data (text section, as well as rw data) into the map.
 */
Mapper::Mapper() {
    int err;
    uint64_t physAddr = 0;
    uintptr_t length = 0, virtAddr = 0;

    log("VM: NX enabled = %s", arch_supports_nx() ? "yes" : "no");

    // placement allocate the kernel map
    gKernelMap = reinterpret_cast<Map *>(&gKernelMapBuf);
    new(gKernelMap) Map(false);

    // map the text segment (R-X)
    err = platform_section_get_info(kSectionKernelText, &physAddr, &virtAddr, &length);
    REQUIRE(!err, "failed to get section %s", "kernel text");

    err = gKernelMap->add(physAddr, length, virtAddr, MapMode::kKernelExec | MapMode::GLOBAL);

    // map the data segment (RW-)
    err = platform_section_get_info(kSectionKernelData, &physAddr, &virtAddr, &length);
    REQUIRE(!err, "failed to get section %s", "kernel data");

    err = gKernelMap->add(physAddr, length, virtAddr, MapMode::kKernelRW | MapMode::GLOBAL);

    // map the bss segment; includes our init stack (RW-)
    err = platform_section_get_info(kSectionKernelBss, &physAddr, &virtAddr, &length);
    REQUIRE(!err, "failed to get section %s", "kernel bss");

    err = gKernelMap->add(physAddr, length, virtAddr, MapMode::kKernelRW | MapMode::GLOBAL);
}



/**
 * Loads a given map into the CPU.
 *
 * @note It's the caller's responsibility to invalidate any addresses that may remain in the MMU
 * translation cache.
 */
void Mapper::loadMap(Map *map) {
    map->activate();
}

/**
 * Convenience helper to load the kernel map into the processor.
 */
void Mapper::loadKernelMap() {
    loadMap(gKernelMap);
}

/**
 * Called after the initial VM map initialization took place, and we've switched over to using that
 * map.
 */
void Mapper::enable() {
    this->vmAvailable = true;
}

/**
 * Returns the kernel map pointer.
 */
Map *Map::kern() {
    return Mapper::gKernelMap;
}
