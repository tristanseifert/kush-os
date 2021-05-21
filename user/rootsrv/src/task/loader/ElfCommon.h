#ifndef TASK_LOADER_ELFCOMMON_H
#define TASK_LOADER_ELFCOMMON_H

#include "Loader.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace task::loader {
/**
 * Base for all ELF binary loaders; provides some common facilities for handling program headers
 * and creating mappings regardless of whether the file is 32 or 64 bit.
 */
class ElfCommon: public Loader {
    public:
        /**
         * Does some basic setup of the common ELF reader.
         */
        ElfCommon(FILE *file) : Loader(file) {};

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


        /// Set up the default stack
        void setUpStack(const std::shared_ptr<Task> &task, const uintptr_t infoStructAddr) override;

    protected:
        virtual size_t getDefaultStackSize() const = 0;
        virtual uintptr_t getDefaultStackAddr() const = 0;

    protected:
        /**
         * Start and end address range (in this process' VM space) for temporary mappings of a
         * page that will be loaded as part of a task
         */
        static const uintptr_t kTempMappingRange[2];

        /// address of the executable entry point
        uintptr_t entryAddr = 0;
        /// bottom address of the stack
        uintptr_t stackBottom = 0;

        /// dynamic linker
        std::string dynLdPath;

        /// index of program headers
        uintptr_t phdrOff = 0;
        /// how many program headers do we have?
        size_t numPhdr = 0;
        /// size of each phdr
        size_t phdrSize = 0;
};
}

#endif
