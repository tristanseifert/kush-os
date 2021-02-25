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

        virtual std::string_view getLoaderId() const override {
            using namespace std::literals;
            return "me.blraaz.exec.loader.elf32"sv;
        }

        /**
         * Gets the entry point of the binary, as read from the ELF header.
         */
        uintptr_t getEntryAddress() const override {
            return this->entryAddr;
        }

        /**
         * The stack is always mapped into a fixed address in each process.
         */
        uintptr_t getStackBottomAddress() const override {
            return kDefaultStackAddr + kDefaultStackSz;
        }

        /**
         * If the executable is dynamic, the dynamic linker needs to be notified.
         */
        bool needsDyldoInsertion() const override {
            return this->isDynamic;
        }

    private:
        void processProgHdr(std::shared_ptr<Task> &, const Elf32_Phdr &);

        void phdrLoad(std::shared_ptr<Task> &, const Elf32_Phdr &);
        void phdrGnuStack(std::shared_ptr<Task> &, const Elf32_Phdr &);
        void phdrInterp(std::shared_ptr<Task> &, const Elf32_Phdr &);

    private:
        /// whether the executable we're loading is statically or dynamically linked
        bool isDynamic = false;
        /// address of the executable entry point
        uintptr_t entryAddr = 0;
};
}

#endif
