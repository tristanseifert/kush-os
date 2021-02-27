#ifndef DYLDO_RUNTIME_DLINFO_H
#define DYLDO_RUNTIME_DLINFO_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <span>

#include <link.h>
#include <sys/elf.h>

namespace dyldo {
class ElfExecReader;
class ElfLibReader;
struct Library;

/**
 * Provides interfaces for the various dynamic linker runtime functions.
 *
 * On initialization, we register symbol overrides for all them so they'll call into our runtime
 * rather than the stubs in the C library.
 */
class DlInfo {
    public:
        DlInfo();

        /// Iterates all loaded objects
        int iterateObjs(int (* _Nonnull callback)(struct dl_phdr_info * _Nonnull, size_t,
                    void* _Nullable), void * _Nullable ctx);

        /// Provides executable info for the dynamic runtime info
        void loadedExec(ElfExecReader * _Nonnull elf, const char * _Nonnull path);
        /// Indicates a library has been loaded.
        void loadedLib(ElfLibReader * _Nonnull elf, Library * _Nonnull lib);

    private:
        struct Object {
            /// path name from which object was loaded from
            const char * _Nonnull path;
            /// program headers of the object
            std::span<Elf32_Phdr> phdrs;

            /// if it's a library, pointer to the library structure
            Library * _Nullable library = nullptr;
        };

    private:
        /// all objects we've loaded
        std::list<Object> loadedObjs;
};
}

#endif
