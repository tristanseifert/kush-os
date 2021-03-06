#ifndef PLATFORM_PC64_IO_SPEW_H
#define PLATFORM_PC64_IO_SPEW_H

namespace platform {
/**
 * Yeets debug data at a 16650-style UART in IO space.
 */
class Spew {
    public:
        static void Init();
        static void WaitTxRdy();
        static void Tx(char ch);
};
}

#endif
