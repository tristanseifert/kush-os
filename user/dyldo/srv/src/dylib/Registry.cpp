#include "Registry.h"
#include "Library.h"

#include <algorithm>

using namespace dylib;

/// shared library registry instance
Registry *Registry::gShared = nullptr;

/**
 * Registers a new library.
 *
 * This call will not add duplicate (e.g. same soname AND path) entries. This means it's not
 * currently possible to replace a dynamic library while processes are running and using it, but
 * that's something we deal with later.
 *
 * @return Whether the library was actually added.
 */
bool Registry::checkAndAdd(std::shared_ptr<Library> &lib, const std::string &path) {
    std::lock_guard<std::mutex> lg(this->librariesLock);

    // insert it directly if we have no library with that soname
    if(!this->libraries.contains(lib->getSoname())) {
        this->actuallyAdd(lib, path);
        return true;
    }
    // otherwise, see if there's a library with the same path as ours
    else {
        // check if there's one with the same load path
        auto samePath = std::count_if(this->libraries.begin(), this->libraries.end(),
                [&](auto &item) -> bool {
            // the soname must match
            const auto [soname, info] = item;
            if(soname != lib->getSoname()) {
                return false;
            }

            // the path must be the same
            return (info.path == path);
        });
        if(samePath > 0) {
            return false;
        }

        // otherwise, insert it directly
        this->actuallyAdd(lib, path);
        return true;
    }
}

/**
 * Adds a library that we know isn't duplicated to the library map.
 */
void Registry::actuallyAdd(std::shared_ptr<Library> &lib, const std::string &path) {
    // build the info struct
    LibInfo i;
    i.lib = lib;
    i.path = path;

    // insert it
    this->libraries.emplace(lib->getSoname(), std::move(i));
}
