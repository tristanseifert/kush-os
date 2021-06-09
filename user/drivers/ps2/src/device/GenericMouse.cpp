#include "GenericMouse.h"
#include "ThreeAxisMouse.h"

#include "Log.h"

/**
 * Initializes the mouse. We'll set its resolution to 4 counts/mm, since the sensitivity scaling
 * is done in software in higher levels.
 */
GenericMouse::GenericMouse(Ps2Controller *controller, const Ps2Port port, const bool tryUpgrade) : 
    Ps2Device(controller, port) {
    // set the resolution of the mouse
    auto cmd = std::make_shared<Ps2Command>(kCommandSetResolution, [tryUpgrade](auto, auto cmd, auto ctx) {
        auto m = reinterpret_cast<GenericMouse *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            if(tryUpgrade) {
                ThreeAxisMouse::TryUpgrade(m);
            }
            // do not attempt to upgrade to Z axis mode
            else {
                m->enableUpdates();
            }
        } else {
            Warn("Failed to set resolution set for device $%p", m);
        }
    }, this);

    cmd->commandPayload = {std::byte{0x02}};
    submit(cmd);
}

/**
 * Performs initialization once the upgrade process changed. This means simply resetting the update
 * rate and enabling updates again.
 */
void GenericMouse::finishInit() {
    this->enableUpdates();
}

/**
 * Disable the mouse again, if possible.
 */
GenericMouse::~GenericMouse() {
    // TODO: implement
}



/**
 * Handles a received mouse byte. It is appended to the packet buffer, and if it becomes full with
 * one whole packet, that packet is processed.
 */
void GenericMouse::handleRx(const std::byte data) {
    if(!this->enabled) return;

    // we sometimes get 0xFA as the first byte here after enabling...
    if(!this->packetBufOff && this->freshlyEnabled && data == Ps2Command::kCommandReplyAck) {
        return;
    }
    // sanity check: the first byte _must_ have bit 3 set
    else if(!this->packetBufOff && (data & std::byte{(1 << 3)}) == std::byte{0}) {
        return;
    }

    // the byte is ostensibly correct so read it
    this->packetBuf[this->packetBufOff++] = data;

    if(this->packetBufOff == this->packetLen) {
        this->handlePacket(std::span{this->packetBuf.begin(),
                this->packetBuf.begin() + this->packetLen});

        this->freshlyEnabled = false;
        this->packetBufOff = 0;
    }
}

/**
 * Handles a received mouse packet.
 *
 * In this case, it handles the standard 3 byte, two coordinate X/Y packet. For mice with scroll
 * wheels or additional buttons, they implement their own version of this method.
 */
void GenericMouse::handlePacket(const std::span<std::byte> &_packet) {
    const auto packet = reinterpret_cast<Packet *>(_packet.data());

    Trace("Button state: %c %c %c, dx %d dy %d", packet->isButtonDown(0) ? 'Y' : 'N',
           packet->isButtonDown(1) ? 'Y' : 'N', packet->isButtonDown(2) ? 'Y' : 'N',
           packet->getDx(), packet->getDy());
}



/**
 * Enables mouse position updates.
 */
void GenericMouse::enableUpdates() {
    // reset internal pointers
    this->packetBufOff = 0;

    // then submit the enable command
    auto cmd = Ps2Command::EnableUpdates([](auto p, auto cmd, auto ctx) {
        auto m = reinterpret_cast<GenericMouse *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            m->freshlyEnabled = true;
            m->enabled = true;
        } else {
            Warn("Failed to %s position updates updates for %p", "enable", m);
        }
    }, this);

    submit(cmd);
}

/**
 * Disable mouse position updates.
 */
void GenericMouse::disableUpdates() {
    auto cmd = Ps2Command::DisableUpdates([](auto p, auto cmd, auto ctx) {
        auto m = reinterpret_cast<GenericMouse *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            m->enabled = false;
        } else {
            Warn("Failed to %s position updates updates for %p", "disable", m);
        }
    }, this);

    submit(cmd);
}
