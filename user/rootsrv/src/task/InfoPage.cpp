#include "InfoPage.h"
#include "Task.h"

#include <sys/syscalls.h>
#include <sys/_infopage.h>

#include <system_error>

using namespace task;

/// shared instance
InfoPage *InfoPage::gShared = nullptr;

/**
 * Allocates the info page and fills in the information we know so far.
 */
InfoPage::InfoPage() {
    int err;

    // allocate a single page of anonymous memory
    err = AllocVirtualAnonRegion(kBaseAddr, kPageLength, VM_REGION_RW, &this->vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    // populate the header of the struct
    this->info = reinterpret_cast<kush_sysinfo_page_t *>(kBaseAddr);
    memset(this->info, 0, kPageLength);

    this->info->version = _KSIP_VERSION_CURRENT;
    this->info->magic = _KSIP_MAGIC;

#if defined(__i386__)
    this->info->pageSz = 0x1000;
#elif defined(__amd64__)
    this->info->pageSz = 0x1000;
#else
#warning Don't know page size for this arch!
#endif
}

/**
 * Maps the info page into a task.
 */
void InfoPage::mapInto(std::shared_ptr<Task> &task) {
    int err;

    err = MapVirtualRegionToFlags(this->vmHandle, task->getHandle(), VM_REGION_READ);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionTo");
    }
}
