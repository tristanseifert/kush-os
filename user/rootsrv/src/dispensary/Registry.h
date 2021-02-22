#ifndef DISPENSARY_REGISTRY_H
#define DISPENSARY_REGISTRY_H

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <string>

namespace dispensary {
class RpcHandler;

/**
 * Underlying storage for the dispensary; this is a thin, thread-safe adapter around a string to
 * port handle mapping.
 */
class Registry {
    friend class RpcHandler;
    friend void RegisterPort(const std::string_view &, const uintptr_t);

    public:
        static void init() {
            gShared = new Registry;
        }

    public:
        /// Adds a new entry to the registry.
        bool registerPort(const std::string_view &key, const uintptr_t port);
        /// Looks up a name and transforms it into a port handle.
        bool lookupPort(const std::string &key, uintptr_t &outHandle);

    private:
        static Registry *gShared;

    private:
        std::mutex lock;
        std::unordered_map<std::string, uintptr_t> storage;
};
}

#endif
