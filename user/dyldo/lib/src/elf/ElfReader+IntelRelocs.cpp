/**
 * Implements relocation types typically used on Intel processors.
 */
#include "ElfReader.h"
#include "Library.h"
#include "Linker.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <sys/elf.h>

using namespace dyldo;

/**
 * Performs i386-style relocations
 */
void ElfReader::patchRelocsi386(const PaddedArray<Elf_Rel> &rels, const uintptr_t base) {
    const SymbolMap::Symbol *symbol;

    // process each relocation
    for(const auto &rel : rels) {
        const auto type = ELF32_R_TYPE(rel.r_info);

        // resolve symbol if needed
        switch(type) {
            // all these require symbol resolution
            case R_386_COPY:
            case R_386_GLOB_DAT:
            case R_386_JMP_SLOT:
            case R_386_32:
            case R_386_TLS_TPOFF:
            case R_386_TLS_DTPMOD32:
            case R_386_TLS_DTPOFF32:
            {
                // translate the symbol index into a name
                const auto symIdx = ELF32_R_SYM(rel.r_info);
                const auto sym = this->symtab[symIdx];
                const auto name = this->readStrtab(sym.st_name);
                if(!name) {
                    Linker::Abort("failed to resolve name for symbol %u (off %x info %x base %x)",
                            symIdx, rel.r_offset, rel.r_info, base);
                }

                // resolve to symbol
                symbol = Linker::the()->resolveSymbol(name);
                if(!symbol) {
                    Linker::Abort("failed to resolve symbol '%s'", name);
                }
                break;
            }

            // no symbol
            default:
                symbol = nullptr;
                break;
        }

        // invoke the processing routine
        switch(type) {
            /**
             * Reads a dword at the specified offset, and add to it our load address, then write
             * it back.
             *
             * This only makes sense in shared libraries.
             */
            case R_386_RELATIVE: {
                uintptr_t value = 0;
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(&value, from, sizeof(value));
                value += base;
                memcpy(from, &value, sizeof(value));
                break;
            }

            /*
             * Copy data from the named symbol, located in a shared library, into our data segment
             * somewhere.
             *
             * After the copy is completed, we override the symbol with the address of the copied
             * data in our data segment. This way, when we perform relocations on shared objects
             * next, they reference this one copy of the symbols, rather than the read-only
             * 'template' of them in their data segment.
             */
            case R_386_COPY: {
                this->relocCopyFromShlib(rel, symbol);
                Linker::the()->overrideSymbol(symbol, rel.r_offset);
                break;
            }

            /**
             * References global data that was previously copied into the app's data segment.
             *
             * This is the complement to the R_386_COPY relocation type.
             */
            case R_386_GLOB_DAT: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &symbol->address, sizeof(uint32_t));
                break;
            }

            /**
             * Updates an entry in the PLT (jump slot) with the address of a symbol.
             */
            case R_386_JMP_SLOT: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &symbol->address, sizeof(uint32_t));
                break;
            }

            /**
             * Write the absolute address of a resolved symbol into the offset specified.
             */
            case R_386_32: {
                uint32_t value = 0;
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(&value, from, sizeof(value));
                value += symbol->address;
                memcpy(from, &value, sizeof(value));
                break;
            }

            /**
             * Thread-local offset for an object. When we look up the symbol, we must add to it the
             * TLS offset for the object, which we acquire from the thread-local handler. This will
             * produce a negative value.
             */
            case R_386_TLS_TPOFF: {
                // read current TLS value
                uint32_t value = 0;
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(&value, from, sizeof(value));

                // add to it the library's TLS offset
                auto tls = Linker::the()->getTls();
                auto off = tls->getLibTlsOffset(symbol->library);
                if(!off) {
                    Linker::Abort("Invalid TLS offset for '%s' in %s: %d", symbol->name,
                            symbol->library->soname, off);
                }

                // XXX: do we need to subtract the exec size?
                //Linker::Trace("Original value: %08x sym %s addr %08x", value, symbol->name, symbol->address);
                value += off - tls->getExecSize() + symbol->address;
                //Linker::Trace("Relocation for '%s': off %d -> %08x", symbol->name, off, value);

                // write it back
                memcpy(from, &value, sizeof(value));
                break;
            }

            /**
             * Reference to a thread-local value in another object
             *
             * This writes the module index in which this thread-local object is defined.
             */
            case R_386_TLS_DTPMOD32: {
                // get module id (in this case, the TLS offset)
                uint32_t value = 0;
                auto tls = Linker::the()->getTls();
                value = tls->getLibTlsOffset(symbol->library);

                // write the value
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(from, &value, sizeof(value));
                break;
            }
            /**
             * Reference to a thread-local value in another object
             *
             * Writes the per-module index of a thread-local variable into the given GOT entry. In
             * this case, it's the raw "address" of the symbol.
             */
            case R_386_TLS_DTPOFF32: {
                uint32_t value = symbol->address;

                // write the value
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &value, sizeof(value));
                break;
            }

            /// unknown relocation type
            default:
                Linker::Abort("unsupported %s relocation: type %u (off %x info %x)", "i386",
                        ELF32_R_TYPE(rel.r_info), rel.r_offset, rel.r_info);
        }
    }
}


/**
 * Performs AMD64 relocations
 */
void ElfReader::patchRelocsAmd64(const PaddedArray<Elf_Rela> &rels, const uintptr_t base) {
    const SymbolMap::Symbol *symbol;

    // ensure stride is valid
    if(rels.elementStride() < sizeof(Elf_Rela)) {
        Linker::Abort("Invalid %s stride: %lu", "Elf_Rela", rels.elementStride());
    }

    // process each relocation
    for(const auto &rel : rels) {
        const auto type = ELF_R_TYPE(rel.r_info);

        // resolve symbol if needed
        switch(type) {
            // all these require symbol resolution
            case R_X86_64_COPY:
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JMP_SLOT:
            case R_X86_64_64:
            case R_X86_64_TPOFF64:
            case R_X86_64_DTPMOD64:
            case R_X86_64_DTPOFF64:
            {
                // translate the symbol index into a name
                const auto symIdx = ELF64_R_SYM(rel.r_info);
                const auto sym = this->symtab[symIdx];

                const auto name = this->readStrtab(sym.st_name);
                if(!name) {
                    Linker::Info("failed to resolve name for symbol %lu (off %lx info %lx base %lx)",
                            symIdx, rel.r_offset, rel.r_info, base);
                    continue;
                }

                // resolve to symbol
                symbol = Linker::the()->resolveSymbol(name);
                if(!symbol) {
                    Linker::Abort("failed to resolve symbol '%s'", name);
                }
                break;
            }

            // no symbol
            default:
                symbol = nullptr;
                break;
        }

        // invoke the processing routine
        switch(type) {
            // B + A
            case R_X86_64_RELATIVE: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                const uint64_t value = base + rel.r_addend;
                memcpy(dest, &value, sizeof(value));
                break;
            }
            // S + A
            case R_X86_64_64: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                const uint64_t value = symbol->address + rel.r_addend;
                memcpy(dest, &value, sizeof(value));
                break;
            }

            /*
             * Copy data from the named symbol, located in a shared library, into our data segment
             * somewhere.
             *
             * After the copy is completed, we override the symbol with the address of the copied
             * data in our data segment. This way, when we perform relocations on shared objects
             * next, they reference this one copy of the symbols, rather than the read-only
             * 'template' of them in their data segment.
             */
            case R_X86_64_COPY: {
                this->relocCopyFromShlib(rel, symbol);
                Linker::the()->overrideSymbol(symbol, rel.r_offset);
                break;
            }

            /**
             * References global data that was previously copied into the app's data segment.
             *
             * This is the complement to the R_X86_64_COPY relocation type.
             */
            case R_X86_64_GLOB_DAT: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &symbol->address, sizeof(uint64_t));
                break;
            }

            /**
             * Updates an entry in the PLT (jump slot) with the address of a symbol.
             */
            case R_X86_64_JMP_SLOT: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &symbol->address, sizeof(uint64_t));
                break;
            }

            /**
             * Thread-local offset for an object. When we look up the symbol, we must add to it the
             * TLS offset for the object, which we acquire from the thread-local handler. This will
             * produce a negative value.
             */
            case R_X86_64_TPOFF64: {
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);

                // add to it the library's TLS offset
                auto tls = Linker::the()->getTls();
                auto off = tls->getLibTlsOffset(symbol->library);
                if(!off) {
                    Linker::Abort("Invalid TLS offset for '%s' in %s: %d", symbol->name,
                            symbol->library->soname, off);
                }

                // calculate offset and write back
                uint64_t value = off - tls->getExecSize() + symbol->address + rel.r_addend;
                memcpy(dest, &value, sizeof(value));
                break;
            }

            /// Writes the module index in which this thread-local object is defined.
            case R_X86_64_DTPMOD64: {
                // get module id (in this case, the TLS offset)
                uint64_t value = 0;
                auto tls = Linker::the()->getTls();
                value = tls->getLibTlsOffset(symbol->library);

                // write the value
                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &value, sizeof(value));
                break;
            }

            /// Writes the offset of a TLS variable in the originating module's TLS block
            case R_X86_64_DTPOFF64: {
                uint64_t value = symbol->address + rel.r_addend;

                auto dest = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(dest, &value, sizeof(value));
                break;
            }

            /// unknown relocation type
            default:
                Linker::Abort("unsupported %s relocation: type %u (off %x info %x addend %x)", "amd64",
                        ELF64_R_TYPE(rel.r_info), rel.r_offset, rel.r_info, rel.r_addend);
        }
    }
}
