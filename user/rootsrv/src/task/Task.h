#ifndef TASK_TASK_H
#define TASK_TASK_H

#include <cstdio>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace task {
namespace loader {
class Loader;
}

class DyldoPipe;

/**
 * Encapsulates information about a task created on the system.
 *
 * These are created for all tasks created via the task creation endpoint. This means that tasks
 * created by directly calling the syscall aren't represented by one of these, but we're really
 * the only task that should create other tasks.
 */
class Task {
    public:
        using Buffer = std::span<std::byte>;

    public:
        // don't call this constructor, use one of the create*() functions
        Task(const std::string &path, const uintptr_t parentTask);
        ~Task();

        /// Returns the task handle for this task
        const uintptr_t getHandle() const {
            return this->taskHandle;
        }
        /// binary path
        const std::string &getPath() const {
            return this->binaryPath;
        }

    public:
        /// Creates a new task from a file
        static uintptr_t createFromFile(const std::string &elfPath,
                const std::vector<std::string> &args, const uintptr_t parent = 0);

    private:
        /// Instantiates a binary loader for the given file
        std::shared_ptr<loader::Loader> getLoaderFor(const std::string &, FILE *);

        /// Builds the launch info structure for this task.
        uintptr_t buildInfoStruct(const std::vector<std::string> &args);

        /// Loads the specified dynamic linker.
        void loadDyld(const std::string &dyldPath, uintptr_t &pcOut);
        /// Completes initialization of a task by performing an userspace return.
        void jumpTo(const uintptr_t pc, const uintptr_t sp);

    private:
        static std::once_flag gDyldoPipeFlag;
        static DyldoPipe *gDyldoPipe;

    private:
        /// path from which the binary was loaded
        std::string binaryPath;

        /// kernel handle for the task
        uintptr_t taskHandle = 0;
};
}

#endif
