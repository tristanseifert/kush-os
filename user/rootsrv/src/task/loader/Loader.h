#ifndef TASK_LOADER_LOADER_H
#define TASK_LOADER_LOADER_H

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <errno.h>
#include <exception>
#include <system_error>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace task {
class Task;
}

namespace task::loader {
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

/**
 * Interface of a binary loader
 */
class Loader {
    public:
        Loader(FILE *_file) : file(_file) {};
        virtual ~Loader() = default;

        /// Gets an identifier of this loader.
        virtual std::string_view getLoaderId() const = 0;

        /// Returns the address of the binary's entry point.
        virtual uintptr_t getEntryAddress() const = 0;
        /// Returns the virtual memory address of the bottom of the entry point stack
        virtual uintptr_t getStackBottomAddress() const = 0;
        /// Whether the dynamic linker needs to be inserted into the task address space.
        virtual bool needsDyld() const = 0;
        /// If the dynamic linker is required, return the path to the linker
        virtual const std::string &getDyldPath() const = 0;

        /// Maps the loadable sections of the ELF into the task.
        virtual void mapInto(std::shared_ptr<Task> &task) {
            this->mapInto(task.get());
        }
        /// Maps the loadable sections of the ELF into the task.
        virtual void mapInto(Task *task) = 0;

        /// Sets up the entry point stack in the given task.
        void setUpStack(std::shared_ptr<Task> &task, const uintptr_t arg = 0) {
            this->setUpStack(task.get(), arg);
        }
        virtual void setUpStack(Task *task, const uintptr_t arg) = 0;

    protected:
        /**
         * Reads the given number of bytes from the file at the specified offset.
         */
        void read(const size_t nBytes, void *out, const uintptr_t offset) {
            int err;

            // seek
            err = fseek(this->file, offset, SEEK_SET);
            if(err) {
                throw std::system_error(errno, std::generic_category(), "Seek failed");
            }

            // read
            err = fread(out, 1, nBytes, this->file);
            if(err == nBytes) {
                // success!
            } else if(err > 0) {
                throw LoaderError(fmt::format("Partial read ({}, expected {})", err, nBytes));
            } else {
                throw std::system_error(errno, std::generic_category(), "Read failed");
            }
        }

    protected:
        /// File handle to this binary. We do NOT own this file, so we don't close it.
        FILE *file = nullptr;
};
}

#endif
