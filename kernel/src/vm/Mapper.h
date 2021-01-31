#ifndef KERNEL_VM_MAPPER_H
#define KERNEL_VM_MAPPER_H

namespace vm {
class Map;

/**
 * Handles creating maps (containers of virtual memory map associations) and controls translation
 * between this abstract format, and the architecture-specific MMU table format.
 */
class Mapper {
    public:
        static void init();
        static void loadKernelMap();

        static void loadMap(Map *map);

    private:
        Mapper();

    private:
        static Mapper *gShared;
};

}

#endif
