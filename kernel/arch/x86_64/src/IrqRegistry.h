#ifndef ARCH_AMD64_IRQREGISTRY_H
#define ARCH_AMD64_IRQREGISTRY_H

#include <stddef.h>
#include <stdint.h>

void pc64_irq_entry(struct amd64_exception_info *);

namespace arch {
class Idt;

/**
 * Maps the CPU physical vector numbers from interrupts to a function to invoke, if any.
 */
class IrqRegistry {
    friend void ::pc64_irq_entry(struct amd64_exception_info *);

    /// Minimum allowable vector number
    constexpr static const uintptr_t kVectorMin = 0x20;
    /// Maximum allowable vector number
    constexpr static const uintptr_t kVectorMax = 0xFF;

    /// Total number of vectors managed in this registry
    constexpr static const size_t kNumVectors = (kVectorMax - kVectorMin) + 1;

    public:
        /// Generic IRQ handler function
        using Handler = void(*)(const uintptr_t vector, void *ctx);

    public:
        /// Creates an IRQ registry for the processor with the given IDT.
        IrqRegistry(Idt *idt);
        /// Installs a handler for the given vector number.
        void install(const uintptr_t vector, Handler h, void *handlerCtx);
        /// Removes a previously installed handler for the given vector number.
        void remove(const uintptr_t vector);

        /// Initialize the IRQ registry for the bootstrap processor
        static void Init();

    private:
        /// Combination of callback function + context for an interrupt handler
        struct HandlerRegistration {
            Handler function;
            void *context = nullptr;
        };

    private:
        /// Handles an IRQ of the given vector number
        void handle(const uintptr_t vector);

    private:
        /// Pointer to the IDT we manipulate
        Idt *idt = nullptr;

        /// registration information
        HandlerRegistration registrations[kNumVectors];
};

extern IrqRegistry *gBspIrqRegistry;
}

#endif
