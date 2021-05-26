#ifndef TASK_LOADER_ELF32_H
#define TASK_LOADER_ELF32_H

#include "Loader.h"
#include "ElfCommon.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>

#include <sys/elf.h>

namespace task::loader {
class Elf32: public ElfCommon {
    public:
        /// Default stack address
        constexpr static const uintptr_t kDefaultStackAddr = 0x01000000;
        /// Default stack size (bytes)
        constexpr static const uintptr_t kDefaultStackSz = 0x20000;

    public:
        Elf32(FILE *file);

        void mapInto(const std::shared_ptr<Task> &task) override;

        virtual std::string_view getLoaderId() const override {
            using namespace std::literals;
            return "me.blraaz.exec.loader.elf32"sv;
        }

    protected:
        size_t getDefaultStackSize() const override {
            return kDefaultStackSz;
        }
        uintptr_t getDefaultStackAddr() const override {
            return kDefaultStackAddr;
        }

    private:
        void processProgHdr(const std::shared_ptr<Task> &, const Elf32_Phdr &);

        void phdrLoad(const std::shared_ptr<Task> &, const Elf32_Phdr &);
        void phdrGnuStack(const std::shared_ptr<Task> &, const Elf32_Phdr &);
        void phdrInterp(const std::shared_ptr<Task> &, const Elf32_Phdr &);
};
}

#endif
