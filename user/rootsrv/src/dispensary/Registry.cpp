#include "Registry.h"

#include "log.h"

#include <thread>

using namespace dispensary;

/// shared instance of the registry
Registry *Registry::gShared = nullptr;

/**
 * Registers a new port. If there was a previous registration for this key, it's overwritten.
 *
 * @return Whether an existing key was overwritten (true) or the registration is new (false)
 */
bool Registry::registerPort(const std::string_view &_key, const uintptr_t port) {
    // copy the key
    const std::string key(_key);

    // get the lock and insert
    std::lock_guard<std::mutex> lg(this->lock);

    LOG("Registered port $%08x'h for '%s'", port, key.c_str());
    bool exists = this->storage.contains(key);

    this->storage[key] = port;

    return exists;
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

/**
 * Looks up the given string in the port map. If the key hasn't been registered yet, waits for
 * up to `wait` amount of time for it to be registered.
 *
 * This is a pretty shitty implementation since we just put the thread for sleep for a bit
 *
 * @param wait How long to wait; if zero, wait forever
 */
bool Registry::lookupPort(const std::string &key, uintptr_t &outHandle,
        const std::chrono::microseconds wait) {
    bool check = true;

    do {
        // is the handle registered
        if(this->lookupPort(key, outHandle)) {
            return true;
        }

        // wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // has timeout expired?
        if(wait.count()) {
            // TODO: implement
        }
    } while(check);

    // if we drop here, timeout expired
    return false;
}
