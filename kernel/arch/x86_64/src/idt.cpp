#include "idt.h"
#include "gdt.h"
#include "exceptions.h"

#include <log.h>
#include <string.h>

using namespace arch;

static void load_idt(void *location, const uint16_t size);

// the descriptor table is allocated in BSS
idt_entry64_t Idt::gIdts[Idt::kNumIdt];


/*
 * Builds the Interrupt Descriptor Table at a fixed location in memory.
 */
void Idt::Init() {
    memset(&gIdts, 0, sizeof(idt_entry64_t)*kNumIdt);

    // install CPU exception handlers
    exception_install_handlers();

    // load the IDT into the processor
    load_idt((void *) &gIdts, sizeof(idt_entry64_t)*kNumIdt);
}


/**
 * Sets the value of an IDT gate.
 *
 * XXX: does this need a lock of some sort?
 *
 * @param entry Index into the IDT to set
 * @param function Address to set the entry to (its offset field)
 * @param segment Code segment to associate with the entry (these must be 64 bit)
 * @param flags Present flag, DPL, and 4-bit type. Should always have 0x80
 * @param stack Interrupt stack to select (out of current TSS) for this interrupt; a value of 0
 * uses the legacy TSS lookup, which we don't support. There are a total of 7 stack slots in the
 * TSS, which are all allocated for each core.
 */
void Idt::Set(const size_t entry, const uintptr_t function, const uintptr_t segment, const uintptr_t flags, const size_t stack) {
    REQUIRE(entry < kNumIdt, "IDT index out of bounds: %u", entry);
    REQUIRE(stack <= 7, "stack offset ouf of bounds: %u", stack);

    gIdts[entry].offset1 = function & 0xFFFF;
    gIdts[entry].offset2 = (function & 0xFFFF0000 >> 16) & 0xFFFF;
    gIdts[entry].selector = segment;
    gIdts[entry].ist = stack;
    gIdts[entry].flags = flags; // OR with 0x60 for user level
    gIdts[entry].reserved = 0;
    gIdts[entry].offset3 = (function & 0xFFFFFFFF00000000) >> 32;
}

/**
 * Loads an interrupt descriptor table.
 */
static void load_idt(void *base, const uint16_t size) {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = size - 1;
    IDTR.base = (uint64_t) base;
    asm volatile("lidt (%0)": : "r"(&IDTR));
}
