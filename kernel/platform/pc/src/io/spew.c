#include "spew.h"

#include <stdbool.h>
#include <platform.h>
#include <arch/x86_io.h>

/**
 * IO port for serial spew
 *
 * Possible values are COM1 (0x3F8), COM2 (0x2F8), COM3 (0x3E8), and COM4 (0x2E8) but the latter
 * two may not be the same for all systems.
 */
#define SPEW_IO_BASE            0x3F8


/**
 * Configures the serial spew port.
 *
 * This is a 16650-style UART, whose IO address base is defined the above define. We default to
 * using COM1. The port is initialized at 115200 baud 8N1.
 */
void serial_spew_init() {
    // no irq's
    io_outb(SPEW_IO_BASE + 1, 0x00);    // Disable all interrupts
    // write the divisor
    io_outb(SPEW_IO_BASE + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    io_outb(SPEW_IO_BASE + 0, 0x00);    // Set divisor to 0 (lo byte) 38400 baud
    io_outb(SPEW_IO_BASE + 1, 0x00);    //                  (hi byte)

    // 8N1
    io_outb(SPEW_IO_BASE + 3, 0x03);    // 8 bits, no parity, one stop bit
    // enable and clear FIFOs. 14 byte threshold
    io_outb(SPEW_IO_BASE + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    io_outb(SPEW_IO_BASE + 4, 0x0B);    // IRQs enabled, RTS/DSR set

    // do a loopback test (write to it)
    io_outb(SPEW_IO_BASE + 4, 0x1E);    // Set in loopback mode, test the serial chip
    io_outb(SPEW_IO_BASE + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // expect to read back the same value
    if(io_inb(SPEW_IO_BASE + 0) != 0xAE) {
        // panic("debug spew port is broken (%04x)", SPEW_IO_BASE);
    }

   // set operation mode
   io_outb(SPEW_IO_BASE + 4, 0x0F);
}

/**
 * Waits for the UART to be ready to accept a character.
 */
void serial_spew_wait_txrdy() {
    bool empty = false;

    do {
        empty = (io_inb(SPEW_IO_BASE + 5) & 0x20) ? true : false;
    } while(!empty);
}

/**
 * Transmits the given character to the serial spew port.
 */
void serial_spew_tx(char ch) {
    io_outb(SPEW_IO_BASE + 0, ch);
}



/**
 * Provide the platform serial spew routine.
 */
void platform_debug_spew(char ch) {
    serial_spew_wait_txrdy();
    serial_spew_tx(ch);
}
