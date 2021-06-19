#ifndef PLATFORM_PC64_IRQ_CORELOCALREGISTRY_H
#define PLATFORM_PC64_IRQ_CORELOCALREGISTRY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace platform {
/**
 * Provides a per-processor interrupt handler registry.
 *
 * This builds on top of the architecture irq handler hooks (which can only support a single irq
 * handler per vector) to call multiple interrupt handlers as well as abstract away the required
 * masking/unmasking and interrupt acknowledgement.
 *
 * It is not allowed to access this from any other core than the one that owns it; doing so will
 * cause data corruption.
 */
class CoreLocalIrqRegistry {
    public:
        using Handler = bool(*)(void *, const uintptr_t);

    public:
        CoreLocalIrqRegistry() {
            memset(&this->registrations, 0, sizeof(registrations));
            memset(&this->isIrqMsi, false, sizeof(isIrqMsi));
        };
        ~CoreLocalIrqRegistry();

        /// Add a new irq handler
        uintptr_t add(const uintptr_t irq, Handler callback, void *callbackCtx);
        /// Remove an existing irq handler
        int remove(const uintptr_t token);

        /// Allocates a vector number for core-local use.
        uintptr_t allocateVector(uintptr_t &outVector);

    private:
        /// Converts an amd64 vector number to an irq number
        constexpr static inline uintptr_t VectorToIrq(const uintptr_t vector) {
            return vector - 48;
        }
        /// Converts an irq number to an amd64 vector number
        constexpr static inline uintptr_t IrqToVector(const uintptr_t irq) {
            return irq + 48;
        }

        /// The first handler for the given vector has been inserted
        void enableIrq(const uintptr_t irq);
        /// The last handler for the given vector has been removed
        void disableIrq(const uintptr_t irq);

        /// IRQ handler function invoked by arch code
        static void ArchIrqEntry(const uintptr_t vector, void *ctx);
        /// Invokes all IRQ handlers for the specified interrupt
        void invokeHandlers(const uintptr_t irq);

    private:
        /// Total number of irq vectors handled
        constexpr static const size_t kNumIrqs = 208;

        /// whether handler add/remove is logged
        static bool gLogHandlers;

        /**
         * Represents information on a single installed handler. These form a linked list for each
         * interrupt vector and are invoked in sequence.
         */
        struct HandlerNode {
            HandlerNode() = default;
            /// Initialize a new node with the given handler AND allocate a token to it
            HandlerNode(Handler fxn, void *ctx) : handler(fxn), handlerCtx(ctx) {
                this->token = __atomic_add_fetch(&gNextToken, 1, __ATOMIC_RELAXED);
            }

            /// this can be used to later remove the handler
            uintptr_t token = 0;

            /// function to invoke
            Handler handler = nullptr;
            /// argument to pass to function
            void *handlerCtx;

            /// if there's additional handlers, they're pointed to by this
            HandlerNode *next = nullptr;

            private:
                /// token to assign to the next interrupt routine
                static uintptr_t gNextToken;
        };

        /// All registered handlers
        HandlerNode *registrations[kNumIrqs];
        /// Whether a particular interrupt has been allocated for use as an MSI vector
        bool isIrqMsi[kNumIrqs];
};
}

#endif
