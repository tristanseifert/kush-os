#ifndef PS2COMMAND_H
#define PS2COMMAND_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class Ps2Controller;
enum class Ps2Port;

/**
 * Encapsulates a command sent to a PS/2 device.
 *
 * Commands consist of a single byte identifier, zero or more auxiliary payload bytes, which are
 * sent to the device. The device will then reply with either an acknowledge or resend byte,
 * plus zero or more bytes of response data.
 */
struct Ps2Command {
    friend class Ps2Controller;

    using Callback = std::function<void(const Ps2Port, Ps2Command * _Nonnull, void * _Nullable)>;
    using CommandPtr = std::shared_ptr<Ps2Command>;

    /// Default value of retries for a command
    constexpr static const size_t kMaxRetries = 3;
    /// Timeout to receive a value for a command (in µS)
    constexpr static const uintptr_t kReplyPayloadTimeout = (1000 * 33);

    /**
     * Enumeration representing the state of the command.
     */
    enum class CompletionState {
        /// Command has not yet been queued
        Initializing                    = 0,
        /// Command has been queued and is pending
        Pending                         = 1,

        /// Sending to device
        Sending                         = 2,

        /// Awaiting acknowledgement (or retry)
        Waiting                         = 3,
        /// Receiving additional reply data
        ReceiveReply                    = 4,

        /// Command was successfully completed
        Acknowledged                    = 5,
        /// Command failed
        Failed                          = 6,
    };

    public:
        /// Command byte to send to device
        std::byte command;
        /// Does the command generate an ACK on success?
        bool commandGeneratesAck = true;
        /// Any additional bytes to go with the command
        std::vector<std::byte> commandPayload;

        /// whether the command completed successfully or not
        CompletionState state{CompletionState::Initializing};
        /// min/max bytes of response expected
        std::pair<size_t, size_t> replyBytesExpected{0, 0};
        /// reply bytes received
        std::vector<std::byte> replyBytes;

        /// Number of times the command has been sent
        size_t numRetries{0};
        /// Maximum number of times the command may be resent
        size_t maxRetries{kMaxRetries};

        /// Callback to be invoked when the command completes
        Callback callback;
        /// Context to be passed to the command completion handler
        void * _Nullable callbackContext{nullptr};

        /// Did the command complete successfully?
        constexpr inline bool didCompleteSuccessfully() const {
            return this->state == CompletionState::Acknowledged;
        }
        /// Determine whether this command might have reply data sent by the device
        constexpr inline bool hasReplyData() const {
            return this->replyBytesExpected.second;
        }
        /// Determine whether the command has a variable sized reply
        constexpr inline bool hasVariableReplySize() const {
            return this->replyBytesExpected.first != this->replyBytesExpected.second;
        }

    public:
        /// Creates a command that will reset the device.
        static auto Reset(const Callback &cb, void * _Nullable context) {
            return std::make_shared<Ps2Command>(kCommandReset, cb, context);
        }
        /// Creates a command that will disable updates from the device.
        static auto DisableUpdates(const Callback &cb, void * _Nullable context) {
            return std::make_shared<Ps2Command>(kCommandDisableUpdates, cb, context);
        }
        /// Creates a command that will enable updates from the device.
        static auto EnableUpdates(const Callback &cb, void * _Nullable context) {
            return std::make_shared<Ps2Command>(kCommandEnableUpdates, cb, context);
        }

        Ps2Command() = default;
        Ps2Command(const std::byte _command, const Callback &cb, void * _Nullable cbCtx) :
            command(_command), callback(cb), callbackContext(cbCtx) {}

        virtual ~Ps2Command() = default;

        /// Command reply byte indicating acknowledgement
        constexpr static const std::byte kCommandReplyAck{0xFA};
        /// Command reply byte indicating the command should be resent
        constexpr static const std::byte kCommandReplyResend{0xFE};

        /// Command byte for the identify command
        constexpr static const std::byte kCommandIdentify{0xF2};
        /// Command byte for the "enable scanning" command
        constexpr static const std::byte kCommandEnableUpdates{0xF4};
        /// Command byte for the "disable scanning" command
        constexpr static const std::byte kCommandDisableUpdates{0xF5};
        /// Command byte for the reset command
        constexpr static const std::byte kCommandReset{0xFF};

    protected:
        /// Marks the command as pending, e.g. it has been accepted for processing
        virtual void markPending() {
            this->state = CompletionState::Pending;
        }

        /// Transmits the command to the device
        virtual void send(const Ps2Port port, Ps2Controller * _Nonnull controller);
        /// Handles a byte of received data, including potential ack
        virtual bool handleRx(const Ps2Port port, Ps2Controller * _Nonnull controller,
                const std::byte data);
        /// Resets the receive timeout
        virtual void resetRxTimeout();

        /// Early completion test for multibyte responses
        virtual bool isReplyComplete();

        /// Invokes the completion handler.
        virtual void complete(const Ps2Port port, Ps2Controller * _Nonnull controller,
                const CompletionState newState);
};

#endif
