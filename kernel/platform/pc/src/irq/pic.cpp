#include "pic.h"

#include <arch/x86_io.h>

/// Master PIC IO address
#define PIC1                    0x20
/// Slave PIC IO address
#define PIC2                    0xA0
#define PIC1_COMMAND            PIC1
#define PIC1_DATA               (PIC1+1)
#define PIC2_COMMAND            PIC2
#define PIC2_DATA               (PIC2+1)

/// Signals an end-of-interrupt to the PIC
#define PIC_EOI                 0x20

#define ICW1_ICW4               0x01 /* ICW4 (not) needed */
#define ICW1_SINGLE             0x02 /* Single (cascade) mode */
#define ICW1_INTERVAL4          0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL              0x08 /* Level triggered (edge) mode */
#define ICW1_INIT               0x10 /* Initialization - required! */

/// Use 8086/88 (MCS-80/85) mode
#define ICW4_8086               0x01
/// Auto (normal) EOI mode
#define ICW4_AUTO               0x02
/// Buffered mode (slave)
#define ICW4_BUF_SLAVE          0x08
/// Buffered mode (master)
#define ICW4_BUF_MASTER         0x0C
/// Special (fully nested, not)
#define ICW4_SFNM               0x10

/// OCW3 irq ready next CMD read
#define PIC_READ_IRR            0x0A
/// OCW3 irq service next CMD read
#define PIC_READ_ISR            0x0B


static void pic_irq_remap(const uint8_t offset1, const uint8_t offset2);

/**
 * Initialize the PIC.
 */
void pic_init() {
    // all we've to do is remap the vectors
    pic_irq_remap(0x20, 0x28);
}



/**
 * Remap the PIC interrupt offsets.
 *
 * @param offset1 Vector offset for the first (master) PIC's interrupts
 * @param offset2 Vector offset for the second (slave) PIC's interrupts
 */
static void pic_irq_remap(const uint8_t offset1, const uint8_t offset2) {
    uint8_t a1, a2;

    a1 = io_inb(PIC1_DATA); // save masks
    a2 = io_inb(PIC2_DATA);

    io_outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    io_wait();
    io_outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
    io_wait();
    io_outb(PIC1_DATA, offset1); // ICW2: Master PIC vector offset
    io_wait();
    io_outb(PIC2_DATA, offset2); // ICW2: Slave PIC vector offset
    io_wait();
    io_outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    io_outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    io_outb(PIC1_DATA, ICW4_8086);
    io_wait();
    io_outb(PIC2_DATA, ICW4_8086);
    io_wait();

    io_outb(PIC1_DATA, a1); // restore saved masks.
    io_outb(PIC2_DATA, a2);
}

/*
 * Sends the End of Interrupt command to the PIC
 */
void pic_irq_eoi(const uint8_t irq) {
    if(irq >= 8) {
        io_outb(PIC2_COMMAND,PIC_EOI);
    }

    io_outb(PIC1_COMMAND,PIC_EOI);
}

/*
 * Disables the PICs.
 */
void pic_irq_disable() {
    io_outb(PIC1_DATA, 0xFF);
    io_wait();
    io_outb(PIC2_DATA, 0xFF);
    io_wait();
}
