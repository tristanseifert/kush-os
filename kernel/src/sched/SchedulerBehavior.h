#ifndef KERNEL_SCHED_SCHEDULERBEHAVIOR_H
#define KERNEL_SCHED_SCHEDULERBEHAVIOR_H

#include "Scheduler.h"

namespace sched {
/**
 * Defines information on how processes in a particular priority band are scheduled.
 *
 * @note This applies to its 'real' priority band, i.e. not taking into account any sort of boosts.
 */
struct SchedulerBehavior {
    /// Maximum boost value
    int16_t maxBoost;
};

/**
 * Array of scheduler behaviors; indices correspond to the values of the Scheduler::PriorityGroup
 * enum.
 */
static const SchedulerBehavior kSchedulerBehaviors[Scheduler::PriorityGroup::kPriorityGroupMax] = {
    // highest priority tasks
    {
        // there's no higher level so no boosting allowed
        .maxBoost = 0,
    },
    // above normal
    {
        // allow the higher few tasks to boost
        .maxBoost = 15,
    },
    // normal
    {
        // allow most tasks to boost up one level
        .maxBoost = 30,
    },
    // below normal
    {
        // allow boosting one level and two for higher priority below normals
        .maxBoost = 50,
    },
    // idle
    {
        // idle tasks may boost all the way to normal
        .maxBoost = 65,
    }
};
};

#endif
