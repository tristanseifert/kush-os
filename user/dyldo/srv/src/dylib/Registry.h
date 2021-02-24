#ifndef DYLIB_REGISTRY_H
#define DYLIB_REGISTRY_H

#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>

namespace dylib {
class Library;

/**
 * Holds references to all loaded dynamic libraries. This allows efficient sharing of the text
 * segments between processes.
 */
class Registry {
    public:
        /// Set up the shared registry instance
        static void init() {
            gShared = new Registry;
        }

        /// Registers a new library.
        static inline bool add(std::shared_ptr<Library> &lib, const std::string &path) {
            return gShared->checkAndAdd(lib, path);
        }

    private:
        bool checkAndAdd(std::shared_ptr<Library> &lib, const std::string &path);
        void actuallyAdd(std::shared_ptr<Library> &, const std::string &);

    private:
        /**
         * Information structure for a loaded library
         */
        struct LibInfo {
            /// path on disk
            std::string path;
            /// reference to the library object
            std::shared_ptr<Library> lib;
        };

    private:
        static Registry *gShared;

        /**
         * Mapping of libraries: the key in this table is the library's SONAME, as extracted from
         * the library itself. Since there may be multiple libraries with the same name, we accept
         * duplicate entries, and then use the fully qualified path of the binary to differentiate
         * them according to the load order.
         */
        std::unordered_multimap<std::string, LibInfo> libraries;
        std::mutex librariesLock;
};
}

#endif
