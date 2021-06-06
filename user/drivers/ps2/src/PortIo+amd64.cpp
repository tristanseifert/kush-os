#include "PortIo.h"
#include "Log.h"

#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>

/**
 * Initializes the port IO structure; the two byte wide ports are added to the task's allow list.
 */
PortIo::PortIo(const uint16_t _cmd, const uint16_t _data) : cmdPort(_cmd), dataPort(_data) {
    int err;

    // add them to allow list
    const uint8_t kBitmap[] = {
        0xFF
    };

    err = Amd64UpdateAllowedIoPorts(kBitmap, 1, _cmd);
    if(err) {
        Abort("%s failed: %d", "Amd64UpdateAllowedIoPorts", err);
    }

    err = Amd64UpdateAllowedIoPorts(kBitmap, 1, _data);
    if(err) {
        Abort("%s failed: %d", "Amd64UpdateAllowedIoPorts", err);
    }
}

/**
 * TODO: remove the command/data port permissions
 */
PortIo::~PortIo() {

}

/**
 * Reads from the given IO port.
 */
uint8_t PortIo::read(const Port p) {
    int err;
    uint8_t temp;

    switch(p) {
        case Port::Command:
            err = Amd64PortReadB(this->cmdPort, 0, &temp);
            break;

        case Port::Data:
            err = Amd64PortReadB(this->dataPort, 0, &temp);
            break;
    }

    if(err) {
        Abort("%s failed: %d", "Amd64PortReadB", err);
    }

    return temp;
}

/**
 * Writes to the given IO port.
 */
void PortIo::write(const Port p, const uint8_t value) {
    int err;

    switch(p) {
        case Port::Command:
            err = Amd64PortWriteB(this->cmdPort, 0, value);
            break;

        case Port::Data:
            err = Amd64PortWriteB(this->dataPort, 0, value);
            break;
    }

    if(err) {
        Abort("%s failed: %d", "Amd64PortWriteB", err);
    }
}
