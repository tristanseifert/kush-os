#ifndef DYLDO_ELF_ELFEXECREADER_H
#define DYLDO_ELF_ELFEXECREADER_H

#include "ElfReader.h"

#include <sys/elf.h>

namespace dyldo {
/**
 * ELF reader specialized for dynamic executables.
 *
 * This supports reading the dynamic symbol table for imported symbols only. (Technically, an ELF
 * executable can export symbols but we don't worry about that for now)
 */
class ElfExecReader: public ElfReader {
    public:
        ElfExecReader(FILE * _Nonnull file);
        ElfExecReader(const char * _Nonnull path);
        virtual ~ElfExecReader();

        /// Returns the entry point address of the executable.
        const uintptr_t getEntryAddress() const {
            return this->entry;
        }

    private:
        void ensureExec();
        void init();

        void loadDynamicInfo();

    private:
        /// entry point of the binary
        uintptr_t entry = 0;
};
}

#endif
