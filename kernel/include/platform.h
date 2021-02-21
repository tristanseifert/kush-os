#ifndef KERN_PLATFORM_H
#define KERN_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

namespace sched {
struct Task;
}

namespace platform {
/**
 * Defines an IRQ level, which is another way to talk about the priority of a given processor
 * core. Lower priority interrupts (whether they come from devices or are generated in software)
 * are pended until higher priority interrupts return.
 */
enum class Irql {
    /**
     * Critical sections
     */
    CriticalSection                     = 7,
    /**
     * General interprocessor interupt
     */
    IPI                                 = 6,
    /**
     * Time keeping interrupt
     */
    Clock                               = 5,
    /**
     * Device interrupts
     */
    DeviceIrq                           = 4,
    /**
     * Scheduler level; you may only call into the scheduler at this level or below.
     */
    Scheduler                           = 3,
    /**
     * Deferred procedure calls are currently executing.
     */
    Dpc                                 = 1,
    /**
     * All higher priority interrupts are permitted.
     */
    Passive                             = 0,
};
}

/**
 * Performs platform-specific initialization.
 */
extern "C" void platform_init();

/**
 * Notifies platform code that virtual memory is available.
 */
void platform_vm_available();

/**
 * Writes a character to the platform debug spew, if any.
 */
void platform_debug_spew(char ch);

/**
 * Panic callback; this should disable interrupts, then halt the machine.
 */
extern "C" void platform_panic_handler() __attribute__((noreturn));



/**
 * Returns the number of physical memory regions that maybe used for memory allocation.
 *
 * @return Number of memory regions, or a negative reror code
 */
int platform_phys_num_regions();

/**
 * Gets information on a physical memory region.
 */
int platform_phys_get_info(const size_t idx, uint64_t *addr, uint64_t *length);



/**
 * Regions of the kernel image that the kernel is interested in. This is used to build the virtual
 * memory maps for the kernel code and data.
 */
typedef enum platform_section {
    kSectionKernelText                  = 1,
    kSectionKernelData                  = 2,
    kSectionKernelBss                   = 3,
    kSectionKernelStack                 = 4,
} platform_section_t;

/**
 * Gets information on the given section, if available.
 *
 * @param physAddr Physical (load) address of the relevant section, or 0 if unavailable
 * @param virtAddr Virtual address of the relevant section
 * @param length Length of the section, in bytes.
 *
 * @return 0 on success.
 */
int platform_section_get_info(const platform_section_t section, uint64_t *physAddr,
        uintptr_t *virtAddr, uintptr_t *length);



/**
 * Acknowledges an interrupt. The provided value is opaque and defined by the platform code. Specifically, you
 * should not assume that 0 indicates no interrupt. Any value you receive as an interrupt token can be passed into
 * this function without causing a panic.
 */
int platform_irq_ack(const uintptr_t token);

/**
 * Raises the interrupt priority level of the current processor. The previous irql is returned.
 */
platform::Irql platform_raise_irql(const platform::Irql irql, const bool enableIrq = true);
/**
 * Lowers the interrupt priority level of the current processor.
 */
void platform_lower_irql(const platform::Irql irql, const bool enableIrq = true);
/**
 * Gets the current irql of the processor.
 */
const platform::Irql platform_get_irql();

/**
 * Requests a dispatch IPI to be sent to the current processor.
 */
void platform_request_dispatch();

/**
 * Ensures we're at the given IRQL.
 */
#define REQUIRE(cond, ...) {if(!(cond)) { panic(__VA_ARGS__); }}
#define REQUIRE_IRQL(irql, ...) { \
    const auto current = platform_get_irql(); \
    if(current != irql) { \
        panic("invalid irql (%d) expected = %d", (int) current, irql); \
    } \
}
#define REQUIRE_IRQL_LEQ(irql, ...) { \
    const auto current = platform_get_irql(); \
    if(current > irql) { \
        panic("invalid irql (%d) expected <= %d", (int) current, irql); \
    } \
}



/**
 * Gets the current system timestamp.
 *
 * System time is kept as the number of nanoseconds since boot-up.
 */
uint64_t platform_timer_now();

/**
 * Registers a new timer callback.
 *
 * The given function is invoked at (or after -- there is no guarantee made as to the resolution of
 * the underlying hardware timers) the given time, in nanoseconds since system boot.
 *
 * Returned is an opaque token that can be used to cancel the timer before it fires. Timers are
 * always one shot.
 *
 * @note The callbacks may be invoked in an interrupt context; you should do as little work as
 * possible there.
 *
 * @return An opaque timer token, or 0 in case an error occurs.
 */
uintptr_t platform_timer_add(const uint64_t at, void (*callback)(const uintptr_t, void *), void *ctx);

/**
 * Removes a previously created timer, if it has not fired yet.
 */
void platform_timer_remove(const uintptr_t token);


/**
 * Called by the idle task when there is no other work to be performed on this processor core. The
 * platform code can use this to place the processor in a low power state.
 */
void platform_idle();

/**
 * Initializes the root server task.
 */
sched::Task *platform_init_rootsrv();


////////////////////////////////////////////////////////////////////////////////
/// Functions below are defined by the kernel.

/// Indicates to the kernel a time tick has taken place.
extern void platform_kern_tick(const uintptr_t irqToken);
/// Invokes the scheduler, in response to a scheduler IPI.
extern void platform_kern_scheduler_update(const uintptr_t irqToken);

#endif
