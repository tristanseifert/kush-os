#ifndef TASK_TASK_H
#define TASK_TASK_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace task {
/**
 * Encapsulates information about a task created on the system.
 *
 * These are created for all tasks created via the task creation endpoint. This means that tasks
 * created by directly calling the syscall aren't represented by one of these, but we're really
 * the only task that should create other tasks.
 */
class Task {
    public:
        // don't call this constructor, use one of the create*() functions
        Task(const std::string &path);
        ~Task();

        /// Returns the task handle for this task
        const uintptr_t getHandle() const {
            return this->taskHandle;
        }

    public:
        /// Creates a new task from memory
        static uintptr_t createFromMemory(const std::string &elfPath,
                const std::span<std::byte> &elf, const std::vector<std::string> &args);


    private:
        /// path from which the binary was loaded
        std::string binaryPath;

        /// kernel handle for the task
        uintptr_t taskHandle;
};
}

#endif
