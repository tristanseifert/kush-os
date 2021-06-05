#include "Task.h"
#include "Registry.h"
#include "InfoPage.h"

#include "LaunchInfo.h"
#include "loader/Loader.h"
#include "loader/Elf32.h"
#include "loader/Elf64.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <system_error>

#include <log.h>
#include <fmt/format.h>
#include <sys/elf.h>
#include <unistd.h>

using namespace task;

std::once_flag Task::gDyldoPipeFlag;
DyldoPipe *Task::gDyldoPipe = nullptr;

const uintptr_t Task::kTempMappingRange[2] = {
    // start
    0x10000000000,
    // end
    0x11000000000,
};

/**
 * Creates a new task, loading the specified file from disk.
 *
 * @return Kernel handle to the task. This can be used with the registry to look up the task
 * object.
 * @throws An exception is thrown if the task could not be loaded.
 */
uintptr_t Task::createFromFile(const std::string &elfPath, const std::vector<std::string> &args,
        const uintptr_t parent) {
    // int err;
    uintptr_t entry = 0;

    // try to open the file
    FILE *fp = fopen(elfPath.c_str(), "rb");
    if(!fp) {
        throw std::system_error(errno, std::generic_category(), "Failed to open executable");
    }

    // create the task object and ensure the shared system pages are mapped
    auto task = std::make_shared<Task>(elfPath, parent);
    InfoPage::gShared->mapInto(task);

    // load the binary into the task's VM map
    auto loader = task->getLoaderFor(elfPath, fp);
    LOG("Loader for %s: %p (id '%s'); task $%p'h", elfPath.c_str(), loader.get(),
            loader->getLoaderId().data(), task->getHandle());

    loader->mapInto(task);

    LOG("loading complete. binary is %s", loader->needsDyld() ? "dynamic" : "static");

    // load dynamic linker as well if needed
    if(loader->needsDyld()) {
        task->loadDyld(loader->getDyldPath(), entry);
    }

    // build the task info structure and push it on the stack
    const auto infoBase = task->buildInfoStruct(args);
    loader->setUpStack(task, infoBase);

    // set up its main thread to jump to the entry point
    Registry::registerTask(task);

    entry = entry ? entry : loader->getEntryAddress();
    task->jumpTo(entry, loader->getStackBottomAddress());

    // clean up
    fclose(fp);
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
std::shared_ptr<loader::Loader> Task::getLoaderFor(const std::string &path, FILE *fp) {
    int err;

    using namespace loader;

    // read the ELF header for the current architecture
    Elf_Ehdr hdr;
    memset(&hdr, 0, sizeof hdr);

    err = fseek(fp, 0, SEEK_SET);
    if(err) {
        throw std::system_error(err, std::generic_category(), "Failed to read ELF header");
    }

    err = fread(&hdr, 1, sizeof hdr, fp);
    if(err != sizeof hdr) {
        throw std::system_error(err, std::generic_category(), "Failed to read ELF header");
    }

    // ensure magic is correct, before we try and instantiate an ELF reader
    err = strncmp(reinterpret_cast<const char *>(hdr.e_ident), ELFMAG, SELFMAG);
    if(err) {
        throw LoaderError(fmt::format("Invalid ELF header: {:02x} {:02x} {:02x} {:02x}",
                    hdr.e_ident[0], hdr.e_ident[1], hdr.e_ident[2], hdr.e_ident[3]));
    }

    // use the class value to pick a reader (32 vs 64 bits)
    switch(hdr.e_ident[EI_CLASS]) {
        case ELFCLASS32:
            return std::dynamic_pointer_cast<loader::Loader>(std::make_shared<Elf32>(fp));
            break;

        case ELFCLASS64:
            return std::dynamic_pointer_cast<loader::Loader>(std::make_shared<Elf64>(fp));
            break;

        default:
            LOG("unsupported ELF class $%02x", hdr.e_ident[EI_CLASS]);
            throw LoaderError(fmt::format("Invalid ELF class: {:02x}", hdr.e_ident[EI_CLASS]));
    }
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
void Task::loadDyld(const std::string &dyldPath, uintptr_t &pcOut) {
    LOG("Loading dynamic linker: '%s'", dyldPath.c_str());

    // open a file handle to it
    FILE *fp = fopen(dyldPath.c_str(), "rb");
    if(!fp) {
        throw std::system_error(errno, std::generic_category(), "Failed to open dynamic linker");
    }

    // load it into the task
    try {
        auto loader = this->getLoaderFor(dyldPath, fp);
        if(loader->needsDyld()) {
            throw loader::LoaderError(fmt::format("Dynamic linker '{}' is not statically linked!",
                        dyldPath));
        }

        loader->mapInto(this->shared_from_this());

        pcOut = loader->getEntryAddress();
    } catch(std::exception &) {
        // ensure the file is closed even on error
        fclose(fp);
        throw;
    }

    // clean up
    fclose(fp);
}

/**
 * Allocates a task information structure.
 */
uintptr_t Task::buildInfoStruct(const std::vector<std::string> &args) {
    int err;
    uintptr_t vmHandle, base;
    std::vector<char> buf;

    static const auto kStrStart = (kLaunchInfoBase + sizeof(kush_task_launchinfo_t));

    // build the header
    kush_task_launchinfo_t info;
    memset(&info, 0, sizeof(kush_task_launchinfo_t));

    info.magic = TASK_LAUNCHINFO_MAGIC;

    // allocate the task path
    info.loadPath = reinterpret_cast<const char *>(kStrStart + buf.size());
    buf.insert(buf.end(), this->binaryPath.begin(), this->binaryPath.end());
    buf.push_back('\0');

    // each of the arguments
    if(!args.empty()) {
        // allocate some space for the arg pointer array
        info.numArgs = args.size();

        std::vector<const char *> argPtrs;
        argPtrs.resize(info.numArgs+1, nullptr);

        // allocate the storage for each of the argument strings
        for(size_t i = 0; i < args.size(); i++) {
            const auto &arg = args[i];

            // update the pointer array
            argPtrs[i] = reinterpret_cast<const char *>(kStrStart + buf.size());

            // copy the string
            buf.insert(buf.end(), arg.begin(), arg.end());
            buf.push_back('\0');
        }

        // set the arg pointers
        info.args = reinterpret_cast<const char **>(kStrStart + buf.size());

        auto argPtrsBytes = reinterpret_cast<const char *>(argPtrs.data());
        const auto argPtrsLen = argPtrs.size() * sizeof(const char *);
        std::copy(argPtrsBytes, argPtrsBytes + argPtrsLen, std::back_inserter(buf));
    }

    // allocate a memory region for it and map it somewhere
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to determine page size");
    }

    const auto totalInfoBytes = sizeof(kush_task_launchinfo_t) + buf.size();
    const auto vmAllocSize = ((totalInfoBytes + pageSz - 1) / pageSz) * pageSz;

    err = AllocVirtualAnonRegion(vmAllocSize, VM_REGION_RW, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    err = MapVirtualRegionRange(vmHandle, Task::kTempMappingRange, vmAllocSize, 0, &base);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    // copy data into it
    auto writePtr = reinterpret_cast<std::byte *>(base);

    memcpy(writePtr, &info, sizeof(kush_task_launchinfo_t));
    writePtr += sizeof(kush_task_launchinfo_t);
    memcpy(writePtr, buf.data(), buf.size());

    // make it read only
    err = VirtualRegionSetFlags(vmHandle, VM_REGION_READ);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualRegionSetFlags");
    }

    // map the page into destination task's address space
    err = MapVirtualRegionRemote(this->taskHandle, vmHandle, kLaunchInfoBase, vmAllocSize,
            VM_REGION_READ);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionRemote");
    }

    // unmap the page from our address space
    err = UnmapVirtualRegion(vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "UnmapVirtualRegion");
    }

    return kLaunchInfoBase;
}
