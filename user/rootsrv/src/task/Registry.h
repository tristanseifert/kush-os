#ifndef TASK_REGISTRY_H
#define TASK_REGISTRY_H

#include <cstddef>
#include <memory>
#include <unordered_map>

#include "Task.h"

#include "log.h"

extern "C" int main(int, const char **);

namespace task {
/**
 * Holds references to all of our task structs, indexed by the task handle.
 */
class Registry {
    friend int ::main(int, const char **);

    public:
        /// Adds a new task to the registry
        static void registerTask(std::shared_ptr<Task> &task) {
            const auto handle = task->getHandle();
            REQUIRE(!gShared->tasks.contains(handle), "attempt to add duplicate task $%08x'h",
                    handle);

            gShared->tasks.emplace(handle, task);
        }

        /// Tests whether we have a task object for the given handle.
        static bool containsTask(const uintptr_t handle) {
            return gShared->tasks.contains(handle);
        }
        /// Returns a reference to the task.
        static auto getTask(const uintptr_t handle) {
            return gShared->tasks.at(handle);
        }

    private:
        /// Initializes the task registry
        static void init() {
            gShared = new Registry;
        }

        static Registry *gShared;

    private:
        std::unordered_map<uintptr_t, std::shared_ptr<Task>> tasks;
};
}

#endif
