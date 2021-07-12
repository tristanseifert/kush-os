#include "ThreeAxisMouse.h"

#include "rpc/EventSubmitter.h"

#include "Ps2IdentifyCommand.h"
#include "Log.h"

/**
 * Attempt to upgrade a generic PS/2 mouse to three axis mode. This is done by setting the sample
 * rate to 200, 100, then 80, and reading out the device ID; it should have now changed from 0x00
 * to 0x03, indicating Z axis mode is active.
 *
 * If Z axis mode is activated, allocate a ThreeAxisMouse object and set it as the device connected
 * to the port of the old mouse object.
 */
void ThreeAxisMouse::TryUpgrade(GenericMouse *mouse) {
    // prepare and send the first command
    auto cmd = std::make_shared<Ps2Command>(GenericMouse::kCommandSetSampleRate,
            ThreeAxisMouse::UpgradeStep1, mouse);

    cmd->commandPayload = {std::byte{200}};
    mouse->submit(cmd);
}

/**
 * Handles the response of the first "set rate" command. If successful, we'll perform the second
 * step of the upgrade sequence and set the rate to 100. If not, we'll finish the generic mouse
 * initialization.
 */
void ThreeAxisMouse::UpgradeStep1(const Ps2Port port, Ps2Command *cmd, void *ctx) {
    auto mouse = reinterpret_cast<GenericMouse *>(ctx);
    if(cmd->didCompleteSuccessfully()) {
        auto cmd = std::make_shared<Ps2Command>(GenericMouse::kCommandSetSampleRate,
                ThreeAxisMouse::UpgradeStep2, mouse);

        cmd->commandPayload = {std::byte{100}};
        mouse->submit(cmd);
    } else {
        Warn("Mouse %p on %s port: %s upgrade step %lu failed", mouse,
                (port == Ps2Port::Primary) ? "primary" : "secondary", "z-axis", 1);
        mouse->finishInit();
    }
}

/**
 * Handles the response of the second step of the upgrade procedure. We'll send the last rate
 * command on success.
 */
void ThreeAxisMouse::UpgradeStep2(const Ps2Port port, Ps2Command *cmd, void *ctx) {
    auto mouse = reinterpret_cast<GenericMouse *>(ctx);
    if(cmd->didCompleteSuccessfully()) {
        auto cmd = std::make_shared<Ps2Command>(GenericMouse::kCommandSetSampleRate,
                ThreeAxisMouse::UpgradeStep3, mouse);

        cmd->commandPayload = {std::byte{80}};
        mouse->submit(cmd);
    } else {
        Warn("Mouse %p on %s port: %s upgrade step %lu failed", mouse,
                (port == Ps2Port::Primary) ? "primary" : "secondary", "z-axis", 2);
        mouse->finishInit();
    }
}

/**
 * Handles the response of the third step of the upgrade procedure. At this point, we'll want to
 * re-read the identification of the device to see if it changed from 0x03 to 0x03.
 */
void ThreeAxisMouse::UpgradeStep3(const Ps2Port port, Ps2Command *cmd, void *ctx) {
    auto mouse = reinterpret_cast<GenericMouse *>(ctx);
    if(cmd->didCompleteSuccessfully()) {
        auto cmd = std::make_shared<Ps2IdentifyCommand>(ThreeAxisMouse::UpgradeIdentReply, mouse);
        mouse->submit(cmd);
    } else {
        Warn("Mouse %p on %s port: %s upgrade step %lu failed", mouse,
                (port == Ps2Port::Primary) ? "primary" : "secondary", "z-axis", 3);
        mouse->finishInit();
    }
}

/**
 * Read the identification provided by the mouse. It should be a single byte, 0x03; if so, we'll
 * allocate a new three axis mouse object that copies from the generic mouse object, and we'll
 * replace it as the device connected to that port.
 *
 * Otherwise, if the ident changed to something else (or the ident failed) we'll abort
 * initialization.
 */
void ThreeAxisMouse::UpgradeIdentReply(const Ps2Port port, Ps2Command *cmd, void *ctx) {
    auto mouse = reinterpret_cast<GenericMouse *>(ctx);
    if(cmd->didCompleteSuccessfully()) {
        // we must have received a single byte of ident data
        if(cmd->replyBytes.size() != 1) goto fail;
        // and it must be 0x03
        else if(cmd->replyBytes[0] != kIdentValue) goto fail;

        // cool, we have a Z axis mouse!
        auto newMouse = std::make_shared<ThreeAxisMouse>(*mouse);

        Success("Z axis acquire on %s port",
               (port == Ps2Port::Primary) ? "primary" : "secondary");
        mouse->controller->setDevice(mouse->port, newMouse);
    } else {
fail:;
        Warn("Mouse %p on %s port: %s upgrade ident failed", mouse,
                (port == Ps2Port::Primary) ? "primary" : "secondary", "z-axis");
        mouse->finishInit();
    }
}



/**
 * Creates a three axis mouse from a given generic mouse. This copies the controller/port pointers
 * and will then enable tracking as normal, after the packet size is configured.
 */
ThreeAxisMouse::ThreeAxisMouse(const GenericMouse &from) : GenericMouse(from.controller, from.port) {
    // set the appropriate packet size fields
    this->packetLen = 4;

    // enable mouse position reporting
    this->enableUpdates();
}

/**
 * Decodes a three axis mouse packet.
 */
void ThreeAxisMouse::handlePacket(const std::span<std::byte> &_packet) {
    const auto packet = reinterpret_cast<ZPacket *>(_packet.data());

    uintptr_t buttons{0};
    for(size_t i = 0; i < 3; i++) {
        if(packet->isButtonDown(i)) buttons |= (1 << i);
    }

    auto es = EventSubmitter::the();
    es->submitMouseEvent(buttons, {packet->getDx(), packet->getDy(), packet->getDz()});
}
