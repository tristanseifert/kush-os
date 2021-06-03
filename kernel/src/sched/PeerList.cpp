#include "PeerList.h"
#include "Scheduler.h"
#include "GlobalState.h"

#include <log.h>
#include <arch.h>
#include <arch/rwlock.h>
#include <arch/PerCpuInfo.h>

using namespace sched;

/// rwlock for the global list of scheduler instances (across all cores)
DECLARE_RWLOCK_S(gSchedulersLock);
/// list of all scheduler instances
rt::Vector<PeerList::InstanceInfo> *PeerList::gSchedulers = nullptr;

/**
 * Initializes the peer list for the given scheduler. It is registered globally, and the peer list
 * is computed. All other schedulers will have their lists invalidated, so that they will be
 * lazily recomputed as needed.
 */
PeerList::PeerList(Scheduler *_sched) : owner(_sched) {
    // insert into the global list of schedulers
    InstanceInfo info(_sched);
    info.coreId = arch::GetProcLocal()->getCoreId();

    RW_LOCK_WRITE(&gSchedulersLock);
    if(!gSchedulers) {
        gSchedulers = new rt::Vector<InstanceInfo>;
    }

    gSchedulers->push_back(info);
    RW_UNLOCK_WRITE(&gSchedulersLock);

    // initialization is done. ensure other schedulers update their peer lists
    this->invalidateOthers();

}

/**
 * Removes our parent scheduler from the global scheduler list, and invalidates all other
 * schedulers' peer lists.
 */
PeerList::~PeerList() {
    // remove the scheduler from the global list
    RW_LOCK_WRITE(&gSchedulersLock);

    for(size_t i = 0; i < gSchedulers->size(); i++) {
        const auto &info = (*gSchedulers)[i];
        if(info.instance != this->owner) continue;

        gSchedulers->remove(i);
        break;
    }

    RW_UNLOCK_WRITE(&gSchedulersLock);

    // invalidate peer list of all other schedulers
    this->invalidateOthers();
}



/**
 * Iterates over the list of all schedulers and marks their peer lists as dirty. This will cause
 * that core to recompute its peers next time it becomes idle and needs to steal work.
 */
void PeerList::invalidateOthers() {
    RW_LOCK_READ(&gSchedulersLock);

    for(const auto &info : *gSchedulers) {
        auto sched = info.instance;
        if(sched == this->owner) continue;

        sched->peers.invalidate();
    }

    RW_UNLOCK_READ(&gSchedulersLock);
}

/**
 * Iterates the list of all schedulers, and produces a version of this sorted in ascending order by
 * the cost of moving a thread from that core. This is used for work stealing.
 *
 * Sorting is by means of an insertion sort. This doesn't scale very well but even for pretty
 * ridiculous core counts this should not be too terribly slow; especially considering this code
 * runs very, very rarely, and even then, only when the core is otherwise idle.
 *
 * @note This may only be called from the core that owns this scheduler, as this is the only core
 * that may access the peer list.
 */
void PeerList::build() {
    // clear the old list of peers
    this->peers.clear();

    RW_LOCK_READ_GUARD(gSchedulersLock);
    const auto numSchedulers = gSchedulers->size();

    // if only one core, we have no peers
    if(numSchedulers == 1) return;
    this->peers.reserve(numSchedulers - 1);

    // iterate over list of peers to copy the infos and insert in sorted order
    const auto myId = arch::GetProcLocal()->getCoreId();

    for(const auto &info : *gSchedulers) {
        const auto cost = platform_core_distance(myId, info.coreId);

        // find index to insert at; it goes immediately before the first with a HIGHER cost
        size_t i = 0;
        for(i = 0; i < this->peers.size(); i++) {
            const auto &peer = this->peers[i];

            if(cost <= platform_core_distance(myId, peer.coreId)) goto beach;
        }

beach:;
        // actually insert it
        this->peers.insert(i, info);
    }

    // clear the dirty flag
    __atomic_clear(&this->dirty, __ATOMIC_ACQUIRE);
}
