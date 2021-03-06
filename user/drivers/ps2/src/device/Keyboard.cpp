#include "Keyboard.h"

#include "rpc/EventSubmitter.h"

#include "Ps2Command.h"
#include "Log.h"

/**
 * Initializes the keyboard.
 * 
 * We'll start by placing it in scan code set 2 and then re-enable scanning so that any characters
 * the user enters will be received.
 */
Keyboard::Keyboard(Ps2Controller *controller, const Ps2Port port) : Ps2Device(controller, port) {
    // submit the set scan code set command
    auto cmd = std::make_shared<Ps2Command>(kCommandSetScanSet, [](auto p, auto cmd, auto ctx) {
        auto kbd = reinterpret_cast<Keyboard *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            kbd->enableUpdates();
        } else {
            Warn("Failed to set scan code set for device $%p", kbd);
        }
    }, this);

    cmd->commandPayload = {std::byte{0x02}}; // set scan code set 2
    submit(cmd);
}

/**
 * Disables scanning of the keyboard, if possible.
 */
Keyboard::~Keyboard() {
    // TODO: implement
}



/**
 * Handles a byte received from the keyboard. We do a little bit of validation before calling into
 * the scancode converter.
 */
void Keyboard::handleRx(const std::byte data) {
    // discard if we're not scanning or it's a reserved byte (we shouldn't receive)
    if(!this->enabled) {
        return Warn("Keyboard %p received byte $%02x %s!", this, data, "but scanning is disabled");
    }
    else if(data == Ps2Command::kCommandReplyAck || data == Ps2Command::kCommandReplyResend) {
        return Warn("Keyboard %p received byte $%02x %s!", this, data, "while in scancode mode");
    }
    // keyboard error?
    if(data == std::byte{0x00} || data == std::byte{0xFF}) {
        return Warn("Keyboard %p error!", this);
    }

    // handle scancode otherwise
    this->handleScancode(data);
}

/**
 * Handles a scan code byte.
 *
 * Parsing these is a little fucked since scan code set 2 really, really sucks. The general idea is
 * that most break codes are prefixed with the $F0 byte. Scan codes are values under $80 if they
 * have not been prefixed by an $E0 or $E1 byte, which indicates the scancode instead is from an
 * alternate set.
 */
void Keyboard::handleScancode(const std::byte data) {
    switch(this->scanState) {
        /**
         * Idle case: this will either place us into the "break" mode (if $F0) is received or the
         * extended key code set if $E0 or $E1 are received.
         */
        case ScanState::Idle:
            if(data == kScancodeEscape1) {
                this->keyFlags |= KeyFlags::ScanCodeAlternate;
                this->scanState = ScanState::ReadKeyOrBreak;
            } else if(data == kScancodeBreak) {
                this->keyFlags |= KeyFlags::Break;
                this->scanState = ScanState::ReadKey;
            }
            // we got a plain scan code :D
            else {
                this->generateKeyEvent(data);
            }
            break;

        /**
         * Expect to receive either a break byte or a scan code.
         */
        case ScanState::ReadKeyOrBreak:
            if(data == kScancodeBreak) {
                this->keyFlags |= KeyFlags::Break;
                this->scanState = ScanState::ReadKey;
            } else {
                this->generateKeyEvent(data);
            }
            break;

        /**
         * Expect to receive a scan code value.
         */
        case ScanState::ReadKey:
            this->generateKeyEvent(data);
            break;
    }
}

/**
 * Generates a key down/up event.
 */
void Keyboard::generateKeyEvent(const std::byte key) {
    auto es = EventSubmitter::the();

    // translate key code and submit it
    const auto scancode = ConvertScancode(key, this->keyFlags);
    if(scancode == -1) {
        Warn("Failed to translate scancode $%02x (flags $%04x)", key,
                static_cast<uint32_t>(this->keyFlags));
        goto beach;
    }

    es->submitKeyEvent(scancode, !TestFlags(this->keyFlags & KeyFlags::Break));

beach:;
    // reset state
    this->keyFlags = KeyFlags::None;
    this->scanState = ScanState::Idle;
}


/**
 * Enables scanning of the keyboard.
 */
void Keyboard::enableUpdates() {
    auto cmd = Ps2Command::EnableUpdates([](auto p, auto cmd, auto ctx) {
        auto kbd = reinterpret_cast<Keyboard *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            kbd->enabled = true;
        } else {
            Warn("Failed to %s scancode updates for %p", "enable", kbd);
        }
    }, this);

    submit(cmd);
}

/**
 * Disables keyboard scanning.
 */
void Keyboard::disableUpdates() {
    auto cmd = Ps2Command::DisableUpdates([](auto p, auto cmd, auto ctx) {
        auto kbd = reinterpret_cast<Keyboard *>(ctx);
        if(cmd->didCompleteSuccessfully()) {
            kbd->enabled = false;
        } else {
            Warn("Failed to %s scancode updates for %p", "disable", kbd);
        }
    }, this);

    submit(cmd);
}



/**
 * Converts the given scancode (and flags) to the generic scancode format. This is done by looking
 * up the values in some big ol tables.
 *
 * @return Translated windowserver scancode or -1 if we don't know it
 */
uint32_t Keyboard::ConvertScancode(const std::byte code, const KeyFlags flags) {
    if(!TestFlags(flags & KeyFlags::ScanCodeAlternate)) {
        if(kScancodePrimary.contains(code)) {
            return static_cast<uint32_t>(kScancodePrimary.at(code));
        }
    } else {
        if(kScancodeAlternate.contains(code)) {
            return static_cast<uint32_t>(kScancodeAlternate.at(code));
        }
    }

    // the scan code was not found in either of the maps
    return -1;
}
