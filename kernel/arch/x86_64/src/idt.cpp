#include "idt.h"
#include "gdt.h"
#include "exceptions.h"

#include <log.h>
#include <string.h>

using namespace arch;

// the descriptor table is allocated in BSS
idt_entry64_t Idt::gIdts[Idt::kNumIdt] __attribute__((aligned((64))));

bool Idt::gLogLoad = false;
bool Idt::gLogSet = false;

/*
 * Builds the Interrupt Descriptor Table at a fixed location in memory.
 */
void Idt::Init() {
    memset(&gIdts, 0, sizeof(idt_entry64_t)*kNumIdt);

    // install CPU exception handlers
    exception_install_handlers();

    // load the IDT into the processor
    Load();
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
void Idt::Set(const size_t entry, const uintptr_t function, const uintptr_t segment, const uintptr_t flags, const Stack stack) {
    REQUIRE(entry < kNumIdt, "IDT index out of bounds: %u", entry);

    if(gLogSet) log("IDT index %2u: addr %016llx segment %04x flags %08x stack %u", entry, 
            function, segment, flags, (uint8_t) stack);

    gIdts[entry].offset1 = function & 0xFFFF;
    gIdts[entry].offset2 = function >> 16;
    gIdts[entry].selector = segment;
    gIdts[entry].ist = static_cast<uint8_t>(stack);
    gIdts[entry].flags = flags; // OR with 0x60 for user level
    gIdts[entry].offset3 = function >> 32ULL;
}

/**
 * Loads the IDT.
 */
void Idt::Load() {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = (sizeof(idt_entry64_t) * kNumIdt) - 1;
    IDTR.base = reinterpret_cast<uintptr_t>(&gIdts);
    asm volatile("lidt (%0)": : "r"(&IDTR));

    if(gLogLoad) log("Loaded IDT %p len %u", IDTR.base, IDTR.length);
}
