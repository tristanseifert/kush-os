#ifndef KERNEL_VM_MAPPER_H
#define KERNEL_VM_MAPPER_H

namespace vm {
class Map;

/**
 * Handles creating maps (containers of virtual memory map associations) and controls translation
 * between this abstract format, and the architecture-specific MMU table format.
 */
class Mapper {
    friend class Map;

    public:
        static void init();
        static void lateInit() {
            gShared->enable();
        }
        static void loadKernelMap();

        static void loadMap(Map *map);

        static const bool isVmAvailable() {
            return gShared->vmAvailable;
        }

    private:
        Mapper();

        void enable();

    private:
        static Mapper *gShared;
        static Map *gKernelMap;

    private:
        /// when set, virtual memory services are available
        bool vmAvailable = false;
};

}

#endif
