extern void __stdstream_init();

/**
 * General C library initialization
 */
void __libc_init() {
    // set up input/output streams
    __stdstream_init();
}

