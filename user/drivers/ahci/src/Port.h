#ifndef AHCIDRV_PORT_H
#define AHCIDRV_PORT_H

#include "AtaCommands.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <variant>

#include <driver/DmaBuffer.h>
#include <driver/ScatterGatherBuffer.h>

class Controller;
class Device;

struct PortReceivedFIS;
struct PortCommandTable;
struct PortCommandList;
struct RegDevToHostFIS;
struct RegHostToDevFIS;

/**
 * Handles transactions for a single port on an AHCI controller.
 *
 * Each port has its own private allocation for the command list and received FIS structures, as
 * well as the command tables. Command tables have a fixed 128-byte header, followed by zero or
 * more physical descriptors, each 16 bytes in length. The amount of memory reserved for each of
 * the command tables correlates directly to the amount of physical descriptors each transfer may
 * consist of.
 *
 * TODO: This needs a _lot_ of locking and other multithreading support :)
 */
class Port: public std::enable_shared_from_this<Port> {
    using DMABufferPtr = std::shared_ptr<libdriver::DmaBuffer>;

    public:
        /**
         * Error codes specific to port operations
         */
        enum Errors: int {
            /// The provided buffer has too many distinct physical regions.
            TooManyExtents                      = -11000,
        };

        /**
         * Encapsulates the completion state of a command sent to the device. A command can either
         * complete successfully or fail; in both cases, there are unique pieces of information
         * available.
         */
        class CommandResult {
            friend class Port;

            private:
                /// Information about a successful command
                struct Success {

                };
                /// Information about a failed command
                struct Failure {
                    /// ATA error register
                    uint8_t ataError{0};
                };

                /// Creates a new command result object
                CommandResult(const uint8_t status) : ataStatus(status) {};

            public:
                /// Is the result successful?
                constexpr bool isSuccess() const {
                    return std::holds_alternative<Success>(this->storage);
                }

                /// Returns the ATA status code
                constexpr uint8_t getAtaStatus() const {
                    return this->ataStatus;
                }
                /// Returns the ATA error code, if the command ended in error
                constexpr uint8_t getAtaError() const {
                    return std::get_if<Failure>(&this->storage)->ataError;
                }

            private:
                /// ATA status register
                uint8_t ataStatus{0};

                /// Stores success and failure specific information
                std::variant<Success, Failure> storage;
        };

        using CommandCallback = std::function<void(const CommandResult&)>;

    public:
        Port(Controller * _Nonnull controller, const uint8_t port);
        virtual ~Port();

        /// Identifies the device attached to the port.
        void probe();
        /// Handle interrupts for this port.
        void handleIrq();

        /// Submit an ATA command with a fixed size response.
        [[nodiscard]] int submitAtaCommand(const AtaCommand cmd, const DMABufferPtr &result,
                const CommandCallback &callback);
        /// Submit an ATA command with the given register state and a fixed size response.
        [[nodiscard]] int submitAtaCommand(const RegHostToDevFIS &regs, const DMABufferPtr &result,
                const CommandCallback &callback);

        /// Returns the controller to which this port belongs
        constexpr auto getController() const {
            return this->parent;
        }
        /// Returns the port number on the controller
        constexpr auto getPortNumber() const {
            return this->port;
        }

    private:
        /**
         * Represents information on a command that's currently in flight. This includes references
         * to all buffers the command requires (so that they aren't deallocated).
         */
        struct CommandInfo {
            /// All buffers referenced by this command
            std::vector<DMABufferPtr> buffers;
            /// callback invoked with command completion info
            CommandCallback callback;

            CommandInfo(const CommandCallback &_callback) : callback(_callback) {}
        };

    private:
        void initCommandTables(const uintptr_t);

        void startCommandProcessing();
        void stopCommandProcessing();

        void identDevice();
        void identSatapiDevice();

        size_t fillCmdTablePhysDescriptors(volatile PortCommandTable * _Nonnull table,
                const DMABufferPtr &buf, const bool irq);

        size_t allocCommandSlot();
        int submitCommand(const uint8_t slot, CommandInfo);
        void completeCommand(const uint8_t slot, const volatile RegDevToHostFIS &regs,
                const bool success);

    private:
        static uintptr_t kPrivateMappingRange[2];

        /// Whether various controller initialization parameters are logged
        constexpr static const bool kLogInit{false};
        /// Whether port IRQs are logged
        constexpr static const bool kLogIrq{false};
        /// Enable to dump command headers of about to be submitted commands to the console
        constexpr static const bool kLogCmdHeaders{false};
        /// Enable to dump all written PRDs to the console
        constexpr static const bool kLogPrds{false};

        /// Offset of command list into the port's private physical memory region
        constexpr static const size_t kCmdListOffset{0};
        /// Offset of the received FIS structure into the port's private physical memory region
        constexpr static const size_t kReceivedFisOffset{0x400};

        /// Offset into the private data region at which the command tables are allocated
        constexpr static const size_t kCommandTableOffset{0x800};
        /// Amount of physical descriptors to reserve for each command
        constexpr static const size_t kCommandTableNumPrds{56};

        /// Port number on the controller
        uint8_t port{0};
        /// VM region handle containing the command list and received FIS structures
        uintptr_t privRegionVmHandle{0};

        /// AHCI controller on which this port is
        Controller * _Nonnull parent;
        /// Device attached to this port
        std::shared_ptr<Device> portDevice;

        /// Received FIS structure for this port
        volatile PortReceivedFIS * _Nonnull receivedFis;
        /// Command list for this port
        volatile PortCommandList * _Nonnull cmdList;

        /// Pointers to the command tables
        std::array<volatile PortCommandTable *, 32> cmdTables;
        /**
         * Bitmask of all commands that have been sent to the device; this is used to determine
         * what commands to look at when an interrupt comes back.
         */
        uint32_t outstandingCommands{0};
        /**
         * Bitmask of busy commands, i.e. the command slots that have been allocated for building a
         * command in to, but may not yet have been finished and sent yet.
         */
        uint32_t busyCommands{0};

        /**
         * Information on any commands that are currently in flight; this includes the buffer(s)
         * they use, and the promise that is to be completed with the result of the command.
         */
        std::array<std::optional<CommandInfo>, 32> inFlightCommands;
        /// Lock on the list of in flight commands
        std::mutex inFlightCommandsLock;
};

#endif
