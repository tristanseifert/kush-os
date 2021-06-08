#include "CoreLocalRegistry.h"
#include "Manager.h"
#include "LocalApic.h"

#include <arch/critical.h>
#include <arch/IrqRegistry.h>
#include <log.h>

using namespace platform;

bool CoreLocalIrqRegistry::gLogHandlers = false;

/// incremented for every new irq handler that's inserted
uintptr_t CoreLocalIrqRegistry::HandlerNode::gNextToken = 0;

/**
 * Removes any outstanding interrupt registrations and masks off all interrupts.
 */
CoreLocalIrqRegistry::~CoreLocalIrqRegistry() {
    // log if there's outstanding registrations
}



/**
 * Installs a new irq handler for the specified interrupt number.
 *
 * If we do not have any registrations for this interrupt, we'll set up the arch handler to point
 * to our interrupt handler stub, then initialize our internal bookkeeping for the interrupt, and
 * then unmask the interrupt. Otherwise, we'll simply add another interrupt handler to the existing
 * list of handlers.
 *
 * Handler methods are invoked with the callback pointer provided, as well as the interrupt number
 * originally registered with. These methods can return `true` to indicate they handled the
 * interrupt and no further routines should be called, or `false` to continue calling other
 * handlers.
 *
 * The interrupt is automatically acknowledged after all handlers are invoked.
 *
 * @return A token that identifies the added registration, or 0 on error
 */
uintptr_t CoreLocalIrqRegistry::add(const uintptr_t irq, Handler callback, void *callbackCtx) {
    DECLARE_CRITICAL();

    // validate arguments
    if(irq >= kNumIrqs || !callback) {
        return 0;
    }

    // allocate the node to be inserted
    auto toInsert = new HandlerNode(callback, callbackCtx);
    bool isFirstHandler = false;

    if(gLogHandlers) {
        log("Irq %3lu: add handler %p(%p) node %p (token $%x)", irq, callback, callbackCtx,
                toInsert, toInsert->token);
    }

    // insert it
    {
        CRITICAL_ENTER();

        // is this the first handler for this vector?
        if(!this->registrations[irq]) {
            this->registrations[irq] = toInsert;
            isFirstHandler = true;
        }
        // it isn't, so insert it at the tail of the list
        else {
            auto next = this->registrations[irq];
            while(next) {
                // if its next pointer is null, insert it here
                if(!next->next) {
                    next->next = toInsert;
                }
                // otherwise, advance to the next element
                else {
                    next = next->next;
                }
            }
        }

        CRITICAL_EXIT();
    }

    // now, enable the interrupt if needed
    if(isFirstHandler) {
        this->enableIrq(irq);
    }

    // return the inserted handler's token
    return toInsert->token;
}

/**
 * Searches all registered interrupt handlers to locate the one with the given token; if found, it
 * is removed.
 *
 * @return 1 if the handler was successfully removed, 0 if it was not found or a negative error.
 */
int CoreLocalIrqRegistry::remove(const uintptr_t token) {
    HandlerNode *found = nullptr;
    uintptr_t foundVec;

    DECLARE_CRITICAL();
    {
        CRITICAL_ENTER();

        // iterate over each vector
        for(uintptr_t i = 0; i < kNumIrqs; i++) {
            HandlerNode *next = this->registrations[i], *prev = nullptr;

            while(next) {
                // token matched?
                if(next->token == token) {
                    found = next;

                    // unlink it from the previous item, if any
                    if(prev) {
                        prev->next = next->next;
                    }
                    // no previous item, so update the root pointer
                    else {
                        this->registrations[i] = next->next;
                    }

                    foundVec = i;
                    goto beach;
                }

                // go to next entry
                prev = next;
                next = next->next;
            }
        }

beach:;
        // disable irq if there's no more handlers
        if(found && !this->registrations[foundVec]) {
            this->disableIrq(foundVec);
        }

        CRITICAL_EXIT();
    }

    // if found, we can now deallocate it and return
    if(found) {
        delete found;
        return 1;
    }

    // otherwise, the handler couldn't be found. it's possibly from another core?
    return 0;
}



/**
 * Enables the given interrupt vector by unmasking it after we've installed the architecture irq
 * handler.
 */
void CoreLocalIrqRegistry::enableIrq(const uintptr_t irq) {
    // install the handler
    arch::IrqRegistry::the()->install(IrqToVector(irq), ArchIrqEntry, this);

    // handler installed, so enable the irq
    IrqManager::the()->setMasked(irq, false);
}

/**
 * Disables the given interrupt vector by masking it and removes the architecture handler.
 */
void CoreLocalIrqRegistry::disableIrq(const uintptr_t irq) {
    // mask the irq first
    IrqManager::the()->setMasked(irq, true);

    // then remove the handler
    arch::IrqRegistry::the()->remove(IrqToVector(irq));
}

/**
 * Interrupt handler, note that the vector is the physical CPU vector, not the logical interrupt
 * number that triggered it.
 */
void CoreLocalIrqRegistry::ArchIrqEntry(const uintptr_t vector, void *ctx) {
    auto reg = reinterpret_cast<CoreLocalIrqRegistry *>(ctx);
    const auto irq = VectorToIrq(vector);
    REQUIRE(irq < kNumIrqs, "invalid vector %lu (irq %lu)", vector, irq);

    // invoke handlers 
    reg->invokeHandlers(irq);

    // acknowledge the irq
    LocalApic::the()->eoi();
}

/**
 * Invokes all handlers registered for the given irq.
 */
void CoreLocalIrqRegistry::invokeHandlers(const uintptr_t irq) {
    bool goNext = true;
    auto node = this->registrations[irq];

    while(node && goNext) {
        goNext = node->handler(node->handlerCtx, irq);

        // go to next
        node = node->next;
    }
}

