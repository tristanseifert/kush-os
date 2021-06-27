#ifndef DYLDO_ELF_ELFLIBREADER_H
#define DYLDO_ELF_ELFLIBREADER_H

#include "ElfReader.h"

#include <list>
#include <vector>

#include <sys/elf.h>

namespace dyldo {
/**
 * ELF reader specialized for shared libraries.
 */
class ElfLibReader: public ElfReader {
    friend class SymbolMap;

    public:
        ElfLibReader(const uintptr_t vmBase, FILE * _Nonnull file, const char * _Nonnull path);
        ElfLibReader(const uintptr_t vmBase, const char * _Nonnull path);
        virtual ~ElfLibReader();

        /// Loads the contents of the library and maps them into memory.
        void mapContents();
        /// Registers all symbols exported from the ELF and registers them.
        void exportSymbols(Library * _Nonnull lib);
        /// Finds all initialization and termination functions and registers them.
        void exportInitFiniFuncs(Library * _Nonnull lib);
        /// Defines the library's thread-local storage requirements.
        void exportThreadLocals(Library * _Nonnull lib);

        void processRelocs(const PaddedArray<Elf_Rel> &rels) override {
            this->patchRelocs(rels, this->base);
        }

        /// Total of VM space required for the library; rounded up to the nearest page.
        const size_t getVmRequirements() const;

    protected:
        /**
         * Shared libraries are always linked as if they're at address 0; so, we simply need to
         * shift all virtual addresses by our load base.
         */
        virtual uintptr_t rebaseVmAddr(const uintptr_t in) const override {
            return (in + this->base);
        }

    private:
        void ensureLib();
        void init();

        void loadDynamicInfo();

    private:
        /// Virtual memory base address at which this library is loaded
        uintptr_t base = 0;
};
}

#endif
