#ifndef PORTIO_H
#define PORTIO_H

#include <cstdint>

/**
 * Provides an interface to access the command and data ports of the 8042.
 */
class PortIo {
    public:
        enum class Port {
            Command, Data
        };

    public:
        PortIo(const uint16_t cmdPort, const uint16_t dataPort);
        ~PortIo();

        uint8_t read(const Port p);
        void write(const Port p, const uint8_t value);

    private:
        /// Port address for command port
        uint16_t cmdPort;
        /// Port address for data port
        uint16_t dataPort;
};

#endif
