#ifndef AHCIDRV_PORT_H
#define AHCIDRV_PORT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <optional>

#include <driver/ScatterGatherBuffer.h>

class Controller;

struct PortReceivedFIS;
struct PortCommandTable;
struct PortCommandList;

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
class Port {
    public:
        Port(Controller * _Nonnull controller, const uint8_t port);
        virtual ~Port();

        /// Handle interrupts for this port.
        void handleIrq();

        /// Submit an ATA command with a fixed size response.
        std::future<void> submitAtaCommand(const uint8_t cmd,
                const std::shared_ptr<libdriver::ScatterGatherBuffer> &result);

    private:
        /**
         * Represents information on a command that's currently in flight. This includes references
         * to all buffers the command requires (so that they aren't deallocated).
         */
        struct CommandInfo {
            /// All buffers referenced by this command
            std::vector<std::shared_ptr<libdriver::ScatterGatherBuffer>> buffers;
            /**
             * Promise that is completed when the command completes; it will either be assigned a
             * void value if it completed successfully, or an exception if there was some sort of
             * issue with the command.
             */
            std::promise<void> promise;

            CommandInfo(std::promise<void> _promise) : promise(std::move(_promise)) {}
        };

    private:
        void initCommandTables(const uintptr_t);

        void startCommandProcessing();
        void stopCommandProcessing();

        void identDevice();

        size_t allocCommandSlot();
        void submitCommand(const uint8_t slot, CommandInfo);

    private:
        static uintptr_t kPrivateMappingRange[2];

        /// Whether various controller initialization parameters are logged
        constexpr static const bool kLogInit{false};
        /// Whether port IRQs are logged
        constexpr static const bool kLogIrq{false};

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
        /// Lock protecting the in flight commands list
        std::mutex inFlightCommandsLock;
};

#endif
