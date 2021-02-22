#include "Registry.h"

#include "log.h"

using namespace dispensary;

/// shared instance of the registry
Registry *Registry::gShared = nullptr;

/**
 * Registers a new port. If there was a previous registration for this key, it's overwritten.
 */
bool Registry::registerPort(const std::string_view &_key, const uintptr_t port) {
    // copy the key
    const std::string key(_key);

    // get the lock and insert
    std::lock_guard<std::mutex> lg(this->lock);

    LOG("Registered port $%08x'h for '%s'", port, key.c_str());
    this->storage[key] = port;

    return true;
}

/**
 * Looks up the given string in the map.
 *
 * @param name Name to search for
 * @param outHandle If the name matches a registration, the corresponding handle.
 *
 * @return Whether the name matched any registered ports.
 */
bool Registry::lookupPort(const std::string &key, uintptr_t &outHandle) {
    std::lock_guard<std::mutex> lg(this->lock);

    // if we contain the key, get its info
    if(this->storage.contains(key)) {
        outHandle = this->storage.at(key);
        return true;
    }

    // does not contain the key
    return false;
}
