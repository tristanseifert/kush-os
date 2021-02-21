#ifndef TASK_LOADER_LOADER_H
#define TASK_LOADER_LOADER_H

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>

namespace task::loader {
/**
 * Interface of a binary loader
 */
class Loader {
    public:
    Loader(const std::span<std::byte> &bytes) : file(bytes) {};
        virtual ~Loader() = default;

    protected:
        /// in-memory copy of the ELF
        std::span<std::byte> file;
};

/**
 * Error surfaced during loading of a binary
 */
class LoaderError: public std::exception {
    public:
        LoaderError();
        LoaderError(const char *what) : whatStr(std::string(what)) {}
        LoaderError(const std::string &what) : whatStr(what) {}

        // returns the error string
        const char *what() const noexcept override {
            return this->whatStr.c_str();
        }

    private:
        /// error description
        std::string whatStr;
};
}

#endif
