#include "Linker.h"
#include "Library.h"
#include "link/SymbolMap.h"
#include "runtime/DlInfo.h"
#include "runtime/ThreadLocal.h"
#include "elf/ElfExecReader.h"
#include "elf/ElfLibReader.h"
#include "struct/PaddedArray.h"

#include <unistd.h>

using namespace dyldo;

extern "C" void __dyldo_jmp_to(const uintptr_t pc, const uintptr_t sp,
        const kush_task_launchinfo_t *info);
uintptr_t __dyldo_stack_start = 0;

/// shared linker instance
Linker *Linker::gShared{nullptr};

#ifdef NDEBUG
bool Linker::gLogTraceEnabled{false};
#else
bool Linker::gLogTraceEnabled{true};
#endif

bool Linker::gLogOpenAttempts{false};
bool Linker::gLogInitFini{false};
bool Linker::gLogTls{false};

/**
 * Initializes a new linker, for the executable at the path given.
 *
 * It's assumed the executable is properly mapped, and as are we, but that's it.
 */
Linker::Linker(const char *_path) {
    // initialize containers
    int err = hashmap_create(1, &this->loaded);
    if(err) {
        Abort("%s failed: %d", "hashmap_create", err);
    }

    // copy path
    this->path = strdup(_path);
    if(!this->path) Abort("out of memory");

    // calculate the slides for libraries and friends
    this->calcSlides();

    // read executable file in
    this->exec = new ElfExecReader(this->path);
    // set up the symbol map
    this->map = new SymbolMap;
}

/**
 * Performs further initialization.
 */
void Linker::secondInit() {
    // set up runtime interfaces 
    this->tls = new ThreadLocal;
    this->dlInfo = new DlInfo;

    // and parse more of the file
    this->exec->parseHeaders();
    this->exec->exportInitFiniFuncs();

    this->dlInfo->loadedExec(this->exec, this->path);
}

/**
 * Calculates the random base addresses for various parts of the runtime.
 */
void Linker::calcSlides() {
#if defined(__i386__)
    // 32-bit x86 does not support sliding the addresses
    this->soBase = 0xA0000000;
#elif defined(__amd64__)
    /*
     * On amd64, we reserve the virtual address space from 0x6800'0000'0000 up to the C library
     * boundary at 0x6FFF'FFFF'FFFF for the dynamic linker runtime.
     *
     * The majority of this is reserved for dynamic libraries, which are allocated a 512G region
     * that is slid on a 2M alignment from 0x6801'0000'0000 to 0x6F80'0000'0000; this gives us
     * roughly 20 bits of entropy here.
     */
    this->soBase = 0x680100000000;
    this->soBase += arc4random_uniform(0x3BF800) * 0x200000ULL;
#else
#error Unknown architecture
#endif

    this->soSlide = this->soBase;
}

/**
 * Calculates the alignment to use to round up the base address of the next library. This is used
 * to introduce another layer of randomness into the load addresses of libraries in lieu of
 * changing the order in which they're loaded.
 */
uintptr_t Linker::calcLibOffset() {
#if defined(__i386__)
    // no variable offsets
    return 0x100000;
#elif defined(__amd64__)
    /*
     * We want to have libraries offset by at least a full page directory (2MB) and up to a maximum
     * of 32M, with page granularity in between.
     */
    uintptr_t offset = 0x200000;
    offset += arc4random_uniform(0x1E00) * 0x1000;
    return offset;
#else
#error Unknown architecture
#endif
}

/**
 * Discards all cached data and releases unneeded memory.
 */
void Linker::cleanUp() {
    /**
     * Ensure all library segments are properly protected; and then get rid of the readers. That
     * will close the file handles as well.
     *
     * We've already extracted all symbol information, and relocations have been performed, so
     * there is no need for anything else.
     */
    hashmap_iterate(&this->loaded, [](void *ctx, void * _lib) -> int {
        auto lib = reinterpret_cast<Library *>(_lib);

        if(lib->reader) {
            lib->reader->applyProtection();

            delete lib->reader;
            lib->reader = nullptr;
        }

        return 1;
    }, this);

    // tear down file readers
    delete this->exec;
    this->exec = nullptr;

    // set up the main thread's thread-local storage here.
    auto tls = this->tls->setUp();
    if(gLogTls) Trace("Main thread tls: %p", tls);
}

/**
 * Performs fixups: in the current implementation, this just performs relocations for all symbols
 * in the executable and dependent libraries.
 *
 * All symbol resolution happens just-in-time by calling back into the runtime linking stub.
 */
void Linker::doFixups() {
    // first, fix up the executable (data and PLT)
    PaddedArray<Elf_Rel> execRels;

    if(this->exec->getDynRels(execRels)) {
        this->exec->processRelocs(execRels);
    }
    if(this->exec->getPltRels(execRels)) {
        this->exec->processRelocs(execRels);
    }

    // then, ALL loaded libraries
    hashmap_iterate(&this->loaded, [](void *ctx, void *value) {
        auto lib = reinterpret_cast<Library *>(value);
        PaddedArray<Elf_Rel> rels;

        if(kLogLibraryFixups) Info("Processing library %s (base %p)", lib->soname, lib->vmBase);

        // update its dynamic relocs
        if(lib->reader->getDynRels(rels)) {
            lib->reader->processRelocs(rels);
        }
        if(lib->reader->getPltRels(rels)) {
            lib->reader->processRelocs(rels);
        }

        return 1;
    }, this);

    // get the entry point address
    this->entryAddr = this->exec->getEntryAddress();
}

/**
 * Jumps to the program entry point.
 */
void Linker::jumpToEntry(const kush_task_launchinfo_t *info) {
    // round up stack address
    const auto stack = ((__dyldo_stack_start + 256 - 1) / 256) * 256;

    __dyldo_jmp_to(this->entryAddr, stack, info);

    // should really never get here...
    abort();
}

/**
 * Runs initializers of all shared libraries (currently, in the same order as they were loaded, 
 * which isn't entirely correct ¯\_(ツ)_/¯) and then those exported by the executable itself.
 */
void Linker::runInit() {
    // run libraries
    hashmap_iterate(&this->loaded, [](void *ctx, void *value) {
        auto lib = reinterpret_cast<Library *>(value);
        if(!lib->initFuncs.empty()) {
            // invoke each function
            if(gLogInitFini) {
                Linker::Trace("Invoking %u init funcs for %s", lib->initFuncs.size(), lib->soname);
            }

            for(auto func : lib->initFuncs) {
                func();
            }

        }

        return 1;
    }, this);

    // run executable
    if(!this->execInitFuncs.empty()) {
        if(gLogInitFini) Linker::Trace("Invoking %u init funcs for executable",
                this->execInitFuncs.size());

        for(auto init : this->execInitFuncs) {
            init();
        }
    }
}

/**
 * Loads dependent libraries.
 *
 * This starts with the ones required by the main executable, recursively loading depenencies of
 * all other libraries until there is nothing left to do.
 */
void Linker::loadLibs() {
    for(const auto &dep : this->exec->getDeps()) {
        this->loadSharedLib(dep.name);
    }
}

/**
 * Loads a dependent library based on its soname. This exits immediately if it's already been
 * loaded.
 */
void Linker::loadSharedLib(const char *soname) {
    int err;

    // bail if it's already been loaded
    if(hashmap_get(&this->loaded, soname, strlen(soname))) {
        return;
    }

    // see if the library exists on disk
    const char *libPath = nullptr;
    auto file = this->openSharedLib(soname, libPath);
    if(!file) {
        Abort("failed to load dependency '%s'", soname);
    }

    // open the library
    const auto base = this->soBase;
    auto loader = new ElfLibReader(base, file, libPath);

    // store its info
    auto info = new Library;
    if(!info) Abort("out of memory");

    info->path = libPath;
    info->base = base;
    info->reader = loader;
    info->soname = strdup(soname);
    if(!info->soname) Abort("out of memory");

    err = hashmap_put(&this->loaded, info->soname, strlen(info->soname), info);
    if(err) {
        Abort("%s failed: %d", "hashmap_put", err);
    }

    /*
     * Map the library into memory.
     *
     * This is required as the first step after we've confirmed it exists so we can get at all of
     * its data structures. All data required for dynamic linking _should_ be covered by a load
     * command.
     */
    loader->mapContents();

    // register the library's VM range
    info->vmBase = base;
    info->vmLength = loader->getVmRequirements();

    /**
     * Get information about all exported symbols in the library. These are extracted from the
     * .dynsym region of the binary.
     *
     * At this stage, we also get its initializers and destructors.
     */
    loader->exportThreadLocals(info);
    loader->exportSymbols(info);
    loader->exportInitFiniFuncs(info);

    // store it in the dynamic info
    this->dlInfo->loadedLib(loader, info);

    // advance the pointer to place the next library
    const auto nextBase = base + loader->getVmRequirements();
    const auto off = this->calcLibOffset();
    this->soBase = ((nextBase + off - 1) / off) * off;

    // process dependencies of the library that was just loaded
    for(const auto &dep : loader->getDeps()) {
        this->loadSharedLib(dep.name);
    }
}

/**
 * Searches for a library with the given soname in the system's default search paths. Currently,
 * the following paths are searched, in order:
 *
 * - /lib
 * - /usr/lib
 * - /usr/local/lib
 * - Directory containing the executable
 *
 * @param soname Object name to search for
 * @param outPath A copy of the path string from which the library was loaded. You are responsible
 * for calling free() on it when no longer needed.
 *
 * @return File handle to the library, or `nullptr` if not found anywhere.
 */
FILE *Linker::openSharedLib(const char *soname, const char* &outPath) {
    constexpr static const size_t kPathBufSz = 300;
    char pathBuf[kPathBufSz];

    // search in each of the pre-defined search directories
    for(const auto &base : kDefaultSearchPaths) {
        memset(pathBuf, 0, kPathBufSz);
        snprintf(pathBuf, kPathBufSz, "%s/%s", base, soname);

        if(gLogOpenAttempts) Trace("trying '%s'", pathBuf);

        FILE *fp = fopen(pathBuf, "rb");
        if(fp) {
            outPath = strdup(pathBuf);
            if(gLogOpenAttempts) Trace("  found library: '%s'", pathBuf);

            return fp;
        }
    }

    // XXX: search in the same directory as the executable

    // failed to find it
    return nullptr;
}

/**
 * Resolves a global symbol.
 */
const SymbolMap::Symbol *Linker::resolveSymbol(const char *name, Library *inLibrary) {
    return this->map->get(name, inLibrary);
}

/**
 * Registers a symbol in the symbol map.
 */
void Linker::exportSymbol(const char * _Nonnull name, const Elf_Sym &sym,
        Library * _Nonnull library) {
    this->map->add(name, sym, library);
}

/**
 * Installs a symbol override.
 */
void Linker::overrideSymbol(const SymbolMap::Symbol *inSym, const uintptr_t newAddr) {
    this->map->addOverride(inSym, newAddr);
}


/**
 * Sets the size of the thread-local region required by the executable.
 */
void Linker::setExecTlsRequirements(const size_t totalLen, const std::span<std::byte> &tdata) {
    this->tls->setExecTlsInfo(totalLen, tdata);
}

/**
 * Sets the thread-local requirements of a shared library.
 */
void Linker::setLibTlsRequirements(const size_t totalLen, const std::span<std::byte> &tdata,
        Library * _Nonnull library) {
    this->tls->setLibTlsInfo(totalLen, tdata, library);
}

/**
 * Prints the base addresses of all loaded images.
 */
void Linker::printImageBases() {
    // executable info
    Info("Entry point: $%p -- %s", this->entryAddr, this->path);

    // all libraries
    Info(" Dylib base: $%p (%lu pages)", this->soSlide, (this->soBase - this->soSlide) / sysconf(_SC_PAGESIZE));
    Info("%-18s %-20s %-10s %s", "Base Address", "Mapping", "Size", "Path");

    hashmap_iterate(&this->loaded, [](void *ctx, void * _lib) -> int {
        auto lib = reinterpret_cast<Library *>(_lib);
        auto linker = reinterpret_cast<Linker *>(ctx);

        Info("$%p: vm <%p, %8lu> %s", lib->base - linker->soSlide, lib->vmBase, lib->vmLength, lib->soname);
        return 1;
    }, this);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
/**
 * Logs a trace message.
 */
#ifdef DYLDO_VERBOSE
void Linker::Trace(const char *str, ...) {
    if(!gLogTraceEnabled) return;
    fputs("\e[34m[dyldo] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stdout, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}
#endif

/**
 * Logs an informational message.
 */
void Linker::Info(const char *str, ...) {
    fputs("\e[33m[dyldo] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}
/**
 * Logs an error and terminates the task.
 */
void Linker::Abort(const char *str, ...) {
    fputs("\e[31m[dyldo] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
    abort();
}
#pragma clang diagnostic pop

