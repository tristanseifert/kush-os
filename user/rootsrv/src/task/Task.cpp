#include "Task.h"
#include "Registry.h"

#include "loader/Loader.h"
#include "loader/Elf32.h"

#include <cstring>

#include <log.h>
#include <fmt/format.h>
#include <sys/elf.h>

using namespace task;

/**
 * Creates a new task with an ELF image in memory.
 *
 * @return Kernel handle to the task. This can be used with the registry to look up the task
 * object.
 * @throws An exception is thrown if the task could not be loaded.
 */
uintptr_t Task::createFromMemory(const std::string &elfPath, const Buffer &elf,
        const std::vector<std::string> &args) {
    // create the task object
    auto task = std::make_shared<Task>(elfPath);

    // load the ELF into the task
    auto loader = task->getLoaderFor(elfPath, elf);
    LOG("Loader for %s: %p", elfPath.c_str(), loader.get());

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

/**
 * Instantiates a loader for the given binary file. Currently, only ELF binaries are supported, but
 * this has the flexibility to support other kinds later.
 *
 * @throw An exception is thrown if the ELF is invalid.
 */
std::shared_ptr<loader::Loader> Task::getLoaderFor(const std::string &path, const Buffer &elf) {
    int err;

    using namespace loader;

    // get at the header
    auto hdrSpan = elf.subspan(0, sizeof(Elf32_Ehdr));
    if(hdrSpan.size() < sizeof(Elf32_Ehdr)) {
        throw LoaderError("ELF header too small");
    }

    auto hdr = reinterpret_cast<const Elf32_Ehdr *>(hdrSpan.data());

    // ensure magic is correct, before we try and instantiate an ELF reader
    err = strncmp(reinterpret_cast<const char *>(hdr->e_ident), ELFMAG, SELFMAG);
    if(err) {
        throw LoaderError(fmt::format("Invalid ELF header: {:02x} {:02x} {:02x} {:02x}",
                    hdr->e_ident[0], hdr->e_ident[1], hdr->e_ident[2], hdr->e_ident[3]));
    }

    // use the class value to pick a reader (32 vs 64 bits)
    if(hdr->e_ident[EI_CLASS] == ELFCLASS32) {
        return std::make_shared<Elf32>(elf);
    } else {
        throw LoaderError(fmt::format("Invalid ELF class: {:02x}", hdr->e_ident[EI_CLASS]));
    }

    // we should not get here
    return nullptr;
}
