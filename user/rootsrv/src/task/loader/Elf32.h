#ifndef TASK_LOADER_ELF32_H
#define TASK_LOADER_ELF32_H

#include "Loader.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include <sys/elf.h>

namespace task::loader {
class Elf32: public Loader {
    public:
        /// Default stack address
        constexpr static const uintptr_t kDefaultStackAddr = 0x01000000;
        /// Default stack size (bytes)
        constexpr static const uintptr_t kDefaultStackSz = 0x10000;

    public:
        Elf32(const std::span<std::byte> &bytes);

        void mapInto(std::shared_ptr<Task> &task) override;
        void setUpStack(std::shared_ptr<Task> &task) override;

        /**
         * Gets the entry point of the binary. For static binaries, this will be the value we read
         * from the ELF header; for dynamic binaries, the entry point is in the dynamic linker,
         * which will pass control to the actual entry point later.
         */
        uintptr_t getEntryAddress() const override {
            if(this->isDynamic) {
                // TODO: get dynamic linker entry point
                return 0;
            } else {
                return this->entryAddr;
            }
        }

        /**
         * The stack is always mapped into a fixed address in each process.
         */
        uintptr_t getStackBottomAddress() const override {
            return kDefaultStackAddr + kDefaultStackSz;
        }

    private:
        void processProgHdr(std::shared_ptr<Task> &, const Elf32_Phdr &);

        void phdrLoad(std::shared_ptr<Task> &, const Elf32_Phdr &);
        void phdrGnuStack(std::shared_ptr<Task> &, const Elf32_Phdr &);

    private:
        /// whether the executable we're loading is statically or dynamically linked
        bool isDynamic = false;
        /// address of the executable entry point
        uintptr_t entryAddr = 0;
};
}

#endif
