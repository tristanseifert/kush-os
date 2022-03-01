#include "Backtrace.h"
#include "Arch/Elf.h"
#include "Boot/Helpers.h"

#include <Runtime/Printf.h>
#include <Runtime/String.h>
#include <Logging/Console.h>
#include <stdint.h>

using namespace Platform::Amd64Uefi;

const void *Backtrace::gSymtab{nullptr};
const char *Backtrace::gStrtab{nullptr};
size_t Backtrace::gSymtabLen{0}, Backtrace::gStrtabLen{0};

// defined by the linker
extern char __kernel_text_start, __kernel_text_end;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/// x86_64 stack frame
struct stackframe {
    struct stackframe *rbp;
    uint64_t rip;
};
#endif

/**
 * Initializes the backtrace facility.
 *
 * This attempts to locate the string and symbol tables in the loaded kernel ELF image. It relies
 * on the entire kernel image being loaded to memory, rather than just the executable sections.
 */
void Backtrace::Init(struct stivale2_struct *loaderInfo) {
    // get the file address
    auto file2 = reinterpret_cast<const struct stivale2_struct_tag_kernel_file_v2 *>(
            Stivale2::GetTag(loaderInfo, STIVALE2_STRUCT_TAG_KERNEL_FILE_V2_ID));
    if(!file2) return;

    // validate ELF header and get the section headers
    auto elfHdr = reinterpret_cast<const Elf64_Ehdr *>(file2->kernel_file);
    if(elfHdr->ident[0] != ELFMAG0 || elfHdr->ident[1] != ELFMAG1 || elfHdr->ident[2] != ELFMAG2
            || elfHdr->ident[3] != ELFMAG3) {
        Kernel::Console::Warning("Invalid ELF magic (%02x %02x %02x %02x)", elfHdr->ident[0],
                elfHdr->ident[1], elfHdr->ident[2], elfHdr->ident[3]);
        return;
    }

    if(sizeof(Elf64_Shdr) < elfHdr->secHdrSize) {
        Kernel::Console::Warning("Invalid ELF section hdr size: %zu", elfHdr->secHdrSize);
        return;
    }

    // iterate through section headers to find the symbol table
    for(size_t i = 0; i < elfHdr->numSecHdr; i++) {
        auto shdr = reinterpret_cast<const Elf64_Shdr *>(file2->kernel_file + elfHdr->secHdrOff +
                (elfHdr->secHdrSize * i));

        // is it the symbol table?
        if(shdr->sh_type == SHT_SYMTAB) {
            gSymtab = reinterpret_cast<const void *>(file2->kernel_file + shdr->sh_offset);
            gSymtabLen = shdr->sh_size;

            // find the string table (via the "link" field)
            if(shdr->sh_link > elfHdr->numSecHdr || !shdr->sh_link) {
                Kernel::Console::Warning("Invalid ELF symbol string table index: %zu", shdr->sh_link);
                return;
            }

            auto strtabShdr = reinterpret_cast<const Elf64_Shdr *>(file2->kernel_file + elfHdr->secHdrOff +
                    (elfHdr->secHdrSize * shdr->sh_link));
            gStrtab = reinterpret_cast<const char *>(file2->kernel_file + strtabShdr->sh_offset);
            gStrtabLen = strtabShdr->sh_size;
        }
    }

    // ensure we've loaded the needed sections
    if(!gSymtab || !gSymtabLen || !gStrtab || !gStrtabLen) {
        Kernel::Console::Warning("Failed to load symbol info: symtab (%p, %lu), strtab (%p, %lu)",
                gSymtab, gSymtabLen, gStrtab, gStrtabLen);
    }
}

/**
 * Prints a backtrace to the given character buffer.
 *
 * @param stack Stack base pointer to start at, or `nullptr` to use the current one
 * @param outBuf Character buffer to write data to
 * @param outBufLen Number of characters the output buffer has space for
 * @param symbolicate Whether we should try to resolve addresses to function names
 * @param skip Number of stack frames at the top to skip
 *
 * @return Number of stack frames output
 */
int Backtrace::Print(const void *stack, char *outBuf, const size_t outBufLen,
        const bool symbolicate, const size_t skip) {
    int numFrames{0};

    // get the initial stack frame
    const struct stackframe *stk;

    if(!stack) {
        asm volatile("movq %%rbp,%0" : "=r"(stk) ::);
    } else {
        stk = reinterpret_cast<const struct stackframe *>(stack);
    }

    if(!stk) return 0;

    // walk the stack
    char *writePtr = outBuf;
    int written;

    constexpr static const size_t kSymbolNameBufLen{100};
    char symbolNameBuf[kSymbolNameBufLen];

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const auto bufAvail = outBufLen - (writePtr - outBuf);
        if(!bufAvail) goto full;

        written = 0;

        // skip if return address is null
        if(!stk->rip) goto next;
        // skip if this is still a frame to skip
        else if(skip && frame < skip) goto next;

        // symbolicate, if requested
        if(symbolicate) {
            written = Symbolicate(stk->rip, symbolNameBuf, kSymbolNameBufLen);
            if(written == 1) {
                written = snprintf_(writePtr, bufAvail, "\n%2zu %016llx %s", frame-skip, stk->rip,
                        symbolNameBuf);
            }
            // regular peasant output
            else {
                goto beach;
            }

        }
        // print raw address
        else {
beach:;
            written = snprintf_(writePtr, bufAvail, "\n%2zu %016llx", frame-skip, stk->rip);
        }

next:
        // prepare for next frame
        writePtr += written;
        stk = stk->rbp;
        numFrames++;

        // bail if this frame is invalid
        if(!stk || !(reinterpret_cast<uintptr_t>(stk) & (1UL << 63))) goto full;
    }

full:
    return numFrames;
}

/**
 * Attempt to symbolicate the provided symbol; it must be inside the kernel.
 *
 * @param pc Symbol address
 * @param outBuf Location where the symbol name is written
 * @param outBufLen Length of symbol name buffer
 *
 * @return 0 if no symbol found, -1 if an error occurred, and 1 if symbol was found.
 */
int Backtrace::Symbolicate(const uintptr_t pc, char *outBuf, const size_t outBufLen) {
    // ensure it's in the kernel .text section
    if(pc < reinterpret_cast<uintptr_t>(&__kernel_text_start) ||
            pc > reinterpret_cast<uintptr_t>(&__kernel_text_end)) {
        return 0;
    }

    // bail if no symbol table
    if(!gSymtab || !gSymtabLen || !gStrtab || !gStrtabLen) {
        return 0;
    }

    // search all symbol entries to find the closest one
    const Elf64_Sym *closest{nullptr};
    uintptr_t closestDiff{UINTPTR_MAX};

    auto syms = reinterpret_cast<const Elf64_Sym *>(gSymtab);
    const auto numSyms = gStrtabLen / sizeof(Elf64_Sym);

    for(size_t i = 0; i < numSyms; i++) {
        const auto &sym = syms[i];

        // skip non-functions
        if(ELF64_ST_TYPE(sym.st_info) != STT_FUNC) continue;

        // this is the closest so store it
        const auto diff = pc - sym.st_value;
        if(!closest || closestDiff > diff) {
            closest = &sym;
            closestDiff = diff;
        }
    }

    // copy out the closest match
    if(closest) {
        const auto name = gStrtab + closest->st_name;
        snprintf_(outBuf, outBufLen, "%s+0x%0llx", name, pc - closest->st_value);
        return 1;
    }

    // no match
    return 0;
}

