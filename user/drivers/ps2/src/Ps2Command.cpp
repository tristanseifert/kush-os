#include "Ps2Command.h"
#include "Ps2Controller.h"
#include "Log.h"

/**
 * Transmits the command byte and any auxiliary bytes to the given device.
 */
void Ps2Command::send(const Ps2Controller::Port port, Ps2Controller *controller) {
    // validate preconditions
    if(this->state != CompletionState::Pending && this->state != CompletionState::Waiting) {
        Abort("Invalid state to send command $%p: %lu", this, static_cast<uintptr_t>(this->state));
    }

    this->state = CompletionState::Sending;

    // transmit the command byte first
    controller->writeDevice(port, this->command);

    // then any auxiliary bytes
    for(const auto byte : this->commandPayload) {
        controller->writeDevice(port, byte);
    }

    // we've finished sending
    this->state = CompletionState::Waiting;
}

/**
 * Handles a byte of received data.
 *
 * We'll receive either an 0xFA byte to indicate the command was acknowledged, in which case we
 * advance to the ReceiveReply state or to completion, if the command does not have any reply
 * bytes. If we receive an 0xFE byte, we'll instead resend the entire command.
 *
 * @return Whether the command is has been completed
 */
bool Ps2Command::handleRx(const Ps2Controller::Port port, Ps2Controller *controller,
        const std::byte data) {
    // we're waiting to receive the ack byte
    if(this->state == CompletionState::Waiting) {
        // the command may not generate an ack byte
        if(this->commandGeneratesAck) {
            // device acknowledges the command
            if(data == kCommandReplyAck) {
                // complete if there are no payload bytes
                if(!this->hasReplyData()) {
                    this->complete(port, controller, CompletionState::Acknowledged);
                    return true;
                }

                // otherwise, move into the payload receive phase
                this->state = CompletionState::ReceiveReply;
                this->resetRxTimeout();
                return false;
            } 
            // device requests command resend
            else if(data == kCommandReplyResend) {
                // failed on number of retries
                if(++this->numRetries >= this->maxRetries) {
                    this->complete(port, controller, CompletionState::Failed);
                    return true;
                }
                // otherwise, resend it
                else {
                    // clear any pending received bytes
                    this->replyBytes.clear();
                    Abort("TODO: implement command resend");
                }
            }
            /*
             * Unknown reply to an acknowledgement; this typically happens when the command is a
             * reset, and the device also returns its identifier after the ack byte. So, this will
             * show up in the next command during initialization, which is always a "disable
             * updates" command. This only really happens with mice, which start out as ID 0x00
             * after a reset so we discard a zero byte.
             */
            else {
                if(this->command == kCommandDisableUpdates && data == std::byte{0x00}) {
                    return false;
                }

                Abort("Unknown command ack byte: $%02x", data);
            }
        }
        // no ack byte generated so handle the general receive byte case
        else {
            if(!this->hasReplyData()) {
                Abort("Received reply byte for command without ack and no payload!");
            }

            this->state = CompletionState::ReceiveReply;
            goto reply;
        }
    }
    // Receive a byte of reply data
    else if(this->state == CompletionState::ReceiveReply) {
reply:;
        this->replyBytes.push_back(data);

        // complete if the reply is the maximum
        if(this->replyBytes.size() == this->replyBytesExpected.second ||
           this->isReplyComplete()) {
            this->complete(port, controller, CompletionState::Acknowledged);
            return true;
        }
        // otherwise, reset the timeout
        else {
            this->resetRxTimeout();
            return false;
        }
    }

    // XXX: should not get down here?
    Abort("Invalid state for receive command $%p: %lu ($%02x)", this,
            static_cast<uintptr_t>(this->state), data);
}

/**
 * Marks a command as completed. The completion handler is invoked.
 */
void Ps2Command::complete(const Ps2Port port, Ps2Controller *controller,
        const CompletionState newState) {
    // TODO: if there are any timeouts, remove them

    // update internal state
    this->state = newState;

    // run callback
    this->callback(port, this, this->callbackContext);
}

/**
 * Resets the receive timeout; if a command can receive a variable number of bytes, we'll reset a
 * timer after each received byte so that the command can time out if fewer than the desired number
 * of bytes have been received.
 *
 * This can then either take the form of an error, or as simply a partial reply.
 */
void Ps2Command::resetRxTimeout() {
    // TODO: implement
}

/**
 * Overrideable method for other commands to determine whether the reply phase of a command should
 * be terminated early, based on the data that has been received so far.
 *
 * @return Whether the reception phase can be completed early.
 */
bool Ps2Command::isReplyComplete() {
    // Give the default behavior forcing full reception/timeout
    return false;
}
