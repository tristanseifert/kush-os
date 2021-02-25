#ifndef TASK_LOADER_ELF32_H
#define TASK_LOADER_ELF32_H

#include "Loader.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>

#include <sys/elf.h>

namespace task::loader {
class Elf32: public Loader {
    public:
        /// Default stack address
        constexpr static const uintptr_t kDefaultStackAddr = 0x01000000;
        /// Default stack size (bytes)
        constexpr static const uintptr_t kDefaultStackSz = 0x10000;

    public:
        Elf32(FILE *file);

        void mapInto(Task *task) override;
        void setUpStack(Task *task, const uintptr_t infoStructAddr) override;

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
            return this->stackBottom;
        }

        /**
         * If the executable is dynamic, the dynamic linker needs to be mapped.
         */
        bool needsDyld() const override {
            return !this->dynLdPath.empty();
        }
        /**
         * The path for the dynamic linker is what we read out as the PT_INTERP field.
         */
        const std::string &getDyldPath() const override {
            return this->dynLdPath;
        }


    private:
        void processProgHdr(Task *, const Elf32_Phdr &);

        void phdrLoad(Task *, const Elf32_Phdr &);
        void phdrGnuStack(Task *, const Elf32_Phdr &);
        void phdrInterp(Task *, const Elf32_Phdr &);

    private:
        /// dynamic linker
        std::string dynLdPath = "";
        /// address of the executable entry point
        uintptr_t entryAddr = 0;

        /// index of program headers
        uintptr_t phdrOff = 0;
        /// how many program headers do we have? (must be sizeof(Elf32_Phdr)!)
        size_t numPhdr = 0;

        /// bottom address of the stack
        uintptr_t stackBottom = 0;
};
}

#endif
