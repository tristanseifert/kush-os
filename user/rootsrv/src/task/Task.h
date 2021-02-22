#ifndef TASK_TASK_H
#define TASK_TASK_H

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace task {
namespace loader {
class Loader;
}

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

    public:
        /// Creates a new task from memory
        static uintptr_t createFromMemory(const std::string &elfPath, const Buffer &elf,
                const std::vector<std::string> &args, const uintptr_t parent = 0);

    private:
        /// Instantiates a binary loader for the given file
        std::shared_ptr<loader::Loader> getLoaderFor(const std::string &, const Buffer &);

        /// Completes initialization of a task by performing an userspace return.
        void jumpTo(const uintptr_t pc, const uintptr_t sp);

    private:
        /// path from which the binary was loaded
        std::string binaryPath;

        /// kernel handle for the task
        uintptr_t taskHandle = 0;
};
}

#endif
