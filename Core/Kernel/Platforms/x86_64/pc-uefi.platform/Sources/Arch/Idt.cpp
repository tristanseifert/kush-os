#include "Idt.h"
#include "IdtTypes.h"
#include "ExceptionHandlers.h"

#include <Logging/Console.h>
#include <Runtime/String.h>

#include <new>

using namespace Platform::Amd64Uefi;

// allocate storage for the BSP IDT
static uint8_t gBspIdtBuf[sizeof(Idt)] __attribute__((aligned(64)));
static Idt *gBspIdt{nullptr};

/**
 * Initialize the bootstrap processor (the current CPU) IDT.
 */
void Idt::InitBsp() {
    gBspIdt = reinterpret_cast<Idt *>(&gBspIdtBuf);
    new(gBspIdt) Idt;
}

/**
 * Sets up an interrupt descriptor table; it's prefilled with the standard exception handlers, but
 * no other vectors are set.
 */
Idt::Idt() {
    memset(this->storage, 0, sizeof(IdtEntry) * kNumIdt);
    ExceptionHandlers::Install(*this);

    this->load();
}

/**
 * Sets the value of an IDT entry.
 *
 * @param entry Index into the IDT to set
 * @param function Address to set the entry to (its offset field)
 * @param segment Code segment to associate with the entry (these must be 64 bit)
 * @param flags Present flag, DPL, and 4-bit type. Should always have 0x80
 * @param stack Interrupt stack to select (out of current TSS) for this interrupt; a value of 0
 *        uses the legacy TSS lookup, which we don't support. There are a total of 7 stack slots in
 *        the TSS, which are all allocated for each core.
 */
void Idt::set(const size_t entry, const uintptr_t function, const uintptr_t segment,
        const uintptr_t flags, const Stack stack) {
    REQUIRE(entry < kNumIdt, "IDT index out of bounds: %u", entry);

    if(kLogSet) Kernel::Console::Debug("IDT %p index %3u: addr $%016llx segment %04x flags %02x "
            "stack %u", this, entry, function, segment, flags, (uint8_t) stack);

    this->storage[entry].offset1 = function & 0xFFFF;
    this->storage[entry].offset2 = function >> 16;
    this->storage[entry].selector = segment;
    this->storage[entry].ist = static_cast<uint8_t>(stack);
    this->storage[entry].flags = flags; // OR with 0x60 for user level
    this->storage[entry].offset3 = function >> 32ULL;
}

/**
 * Loads the IDT.
 */
void Idt::load() {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = (sizeof(IdtEntry) * kNumIdt) - 1;
    IDTR.base = reinterpret_cast<uintptr_t>(&this->storage);

    asm volatile("lidt (%0)": : "r"(&IDTR));

    if(kLogLoad) Kernel::Console::Debug("Loaded IDT %p len %u", IDTR.base, IDTR.length);
}
