#ifndef PLATFORM_PC_IO_SPEW_H
#define PLATFORM_PC_IO_SPEW_H

/**
 * Initializes the serial spew.
 */
void serial_spew_init();

/**
 * Waits for the serial spew port to be ready to send a character.
 */
void serial_spew_wait_txrdy();

/**
 * Sends a character through the serial spew port.
 */
void serial_spew_tx(char ch);

#endif
