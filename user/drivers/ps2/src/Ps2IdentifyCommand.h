#ifndef PS2IDENTIFYCOMMAND_H
#define PS2IDENTIFYCOMMAND_H

#include "Ps2Command.h"
#include "Log.h"
#include <algorithm>
#include <array>
#include <cstddef>

/**
 * Implements the identify command; it can terminate the read early if we detect one of the single
 * byte ID codes.
 */
struct Ps2IdentifyCommand: public Ps2Command {
    public:
        /**
         * Create a new identify command.
         */
        Ps2IdentifyCommand(const Ps2Command::Callback &cb, void * _Nullable ctx) :
            Ps2Command(Ps2Command::kCommandIdentify, cb, ctx) {
            this->replyBytesExpected.second = 2;
        }

    protected:
        /**
         * Allow the ID read command to exit early if the single ID code has been recognized.
         */
        bool isReplyComplete() override {
            // ignore if 0xFA
            if(this->replyBytes.back() == Ps2Command::kCommandReplyAck) {
                this->replyBytes.pop_back();
            }

            if(this->replyBytes.size() == 1) {
                const auto x = this->replyBytes[0];
                return (std::find(gKnownIds.begin(), gKnownIds.end(), x) != gKnownIds.end());
            }
            // otherwise, force the entire read to complete
            return false;
        }

    private:
        static constexpr const std::array<std::byte, 3> gKnownIds{
            std::byte{0x00}, std::byte{0x03}, std::byte{0x04}
        };
};

#endif
