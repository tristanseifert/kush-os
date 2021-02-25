#include "Task.h"
#include "Registry.h"
#include "InfoPage.h"
#include "DyldoPipe.h"

#include "loader/Loader.h"
#include "loader/Elf32.h"

#include <cstring>
#include <system_error>

#include <log.h>
#include <fmt/format.h>
#include <sys/elf.h>

using namespace task;

std::once_flag Task::gDyldoPipeFlag;
DyldoPipe *Task::gDyldoPipe = nullptr;


/**
 * Creates a new task with an ELF image in memory.
 *
 * @return Kernel handle to the task. This can be used with the registry to look up the task
 * object.
 * @throws An exception is thrown if the task could not be loaded.
 */
uintptr_t Task::createFromMemory(const std::string &elfPath, const Buffer &elf,
        const std::vector<std::string> &args, const uintptr_t parent) {
    uintptr_t entry = 0;

    // create the task object and ensure the shared system pages are mapped
    auto task = std::make_shared<Task>(elfPath, parent);
    InfoPage::gShared->mapInto(task);

    // load the ELF into the task
    auto loader = task->getLoaderFor(elfPath, elf);
    LOG("Loader for %s: %p (id '%s'); task $%08x'h", elfPath.c_str(), loader.get(),
            loader->getLoaderId().data(), task->getHandle());

    loader->mapInto(task);

    // set up a stack and map the dynamic linker stubs if enabled
    loader->setUpStack(task);
    if(loader->needsDyldoInsertion()) {
        task->notifyDyldo(entry);
    }

    // add to registry
    Registry::registerTask(task);

    // set up its main thread to jump to the entry point
    entry = entry ? entry : loader->getEntryAddress();
    task->jumpTo(entry, loader->getStackBottomAddress());

    // if we get here, the task has been created
    return task->getHandle();
}



/**
 * Creates a new task object.
 *
 * Here we create the task object and prepare to map executable pages in it.
 */
Task::Task(const std::string &path, const uintptr_t parentTask) : binaryPath(path) {
    int err;

    // create the task
    LOG("Creating task '%s'", path.c_str());
    err = TaskCreateWithParent(parentTask, &this->taskHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "TaskCreate");
    }

    // set its name
    std::string name = path;

    if(name.find_first_of('/') != std::string::npos) {
        name = name.substr(name.find_last_of('/')+1);
    }

    err = TaskSetName(this->taskHandle, name.c_str());
    if(err) {
        throw std::system_error(err, std::generic_category(), "TaskSetName");
    }
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

/**
 * Uses the TaskInitialize() syscall to execute a return to user mode.
 */
void Task::jumpTo(const uintptr_t pc, const uintptr_t sp) {
    int err = TaskInitialize(this->taskHandle, pc, sp);
    if(err) {
        throw std::system_error(err, std::generic_category(), "TaskInitialize");
    }
}

/**
 * Notifies the dynamic linker that this task has been loaded, and the dynamic libraries should
 * be mapped into it. It also returns the actual entry point to jump to, in place of the one
 * listed in the binary header.
 */
void Task::notifyDyldo(uintptr_t &newPc) {
    int err;

    // set up the dyldo communicator
    std::call_once(gDyldoPipeFlag, []() {
        gDyldoPipe = new DyldoPipe;
    });

    // send task notify
    err = gDyldoPipe->taskLaunched(this, newPc);

    if(err) {
        // XXX: handle this better. this will crash the task on launch
        LOG("Failed to notify dyldo of task %p loading: %d", this, err);
        newPc = 1;
    }
}
