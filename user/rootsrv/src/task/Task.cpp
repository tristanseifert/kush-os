#include "Task.h"
#include "Registry.h"

#include <log.h>

using namespace task;

/**
 * Creates a new task with an ELF image in memory.
 *
 * @return Kernel handle to the task. This can be used with the registry to look up the task
 * object. In case of an error, the value zero is returned.
 */
uintptr_t Task::createFromMemory(const std::string &elfPath, const std::span<std::byte> &elf,
        const std::vector<std::string> &args) {
    // create the task object
    auto task = std::make_shared<Task>(elfPath);

    // load the ELF into the task

    // set up a stack and map the dynamic linker stubs if enabled

    // add to registry
    Registry::registerTask(task);

    // set up its main thread to jump to the entry point

    // if we get here, the task has been created
    return task->getHandle();
}



/**
 * Creates a new task object.
 *
 * Here we create the task object and prepare to map executable pages in it.
 */
Task::Task(const std::string &path) : binaryPath(path) {

}

/**
 * Releases all internal structures associated with the task.
 *
 * By the time this is called, the task we represents has terminated: we garbage collect these
 * structures periodically by checking what tasks have terminated.
 */
Task::~Task() {

}
