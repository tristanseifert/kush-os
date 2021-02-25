#include "Linker.h"

#include "elf/ElfExecReader.h"
#include "elf/ElfLibReader.h"

using namespace dyldo;

#ifdef NDEBUG
bool Linker::gLogTraceEnabled = false;
#else
bool Linker::gLogTraceEnabled = true;
#endif

/**
 * Initializes a new linker, for the executable at the path given.
 *
 * It's assumed the executable is properly mapped, and as are we, but that's it.
 */
Linker::Linker(const char *path) {
    // initialize containers
    int err = hashmap_create(1, &this->loaded);
    if(err) {
        Abort("hashmap_create failed: %d", err);
    }

    // load the exec file and its dependencies
    this->exec = new ElfExecReader(path);
}

/**
 * Discards all cached data and releases unneeded memory.
 */
void Linker::cleanUp() {
    // destroy hash map
    hashmap_destroy(&this->loaded);

    // tear down file readers
    delete this->exec;
    this->exec = nullptr;
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
    auto file = this->openSharedLib(soname);
    if(!file) {
        Abort("failed to load dependency '%s'", soname);
    }

    // open the library
    const auto base = this->soBase;
    auto loader = new ElfLibReader(base, file);

    // store its info
    auto info = new Library;
    if(!info) Abort("out of memory");

    info->base = base;
    info->reader = loader;

    err = hashmap_put(&this->loaded, soname, strlen(soname), info);
    if(err) {
        Abort("hashmap_put failed: %d", err);
    }

    /*
     * Map the library into memory.
     *
     * This is required as the first step after we've confirmed it exists so we can get at all of
     * its data structures. All data required for dynamic linking _should_ be covered by a load
     * command.
     */
    loader->mapContents();

    // advance the pointer to place the next library
    const auto nextBase = base + loader->getVmRequirements();
    this->soBase = ((nextBase + kLibAlignment - 1) / kLibAlignment) * kLibAlignment;

    // process dependencies of the library that was just loaded
    for(const auto &dep : loader->getDeps()) {
        Trace("Dep for %s: %s", soname, dep.name);
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
 * @return File handle to the library, or `nullptr` if not found anywhere.
 */
FILE *Linker::openSharedLib(const char *soname) {
    constexpr static const size_t kPathBufSz = 300;
    char pathBuf[kPathBufSz];

    // search in each of the pre-defined search directories
    for(const auto &base : kDefaultSearchPaths) {
        memset(pathBuf, 0, kPathBufSz);
        snprintf(pathBuf, kPathBufSz, "%s/%s", base, soname);

        Trace("trying '%s'", pathBuf);

        FILE *fp = fopen(pathBuf, "rb");
        if(fp) {
            return fp;
        }
    }

    // XXX: search in the same directory as the executable

    // failed to find it
    return nullptr;
}



#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
/**
 * Logs a trace message.
 */
void Linker::Trace(const char *str, ...) {
    if(!gLogTraceEnabled) return;
    fputs("\e[34m[dyldo] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stdout, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}

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
    _Exit(-69);
}
#pragma clang diagnostic pop

