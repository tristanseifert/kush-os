#ifndef PS2CONTROLLER_H
#define PS2CONTROLLER_H

#include "PortIo.h"
#include "Ps2Command.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>
#include <queue>

struct mpack_node_t;
struct acpi_resource_io;

class Ps2Device;
class PortDetector;

/**
 * A port to which a PS/2 device may be connected
 */
enum class Ps2Port {
    // Port 1 (usually for keyboard)
    Primary,
    // Port 2 (usually for mouse)
    Secondary,
};

/**
 * Encapsulates the 8042 PS/2 controller. This supports the most commonly found dual-port variant
 * of the controller.
 */
class Ps2Controller {
    public:
        /// Selects one of the ports on the controller
        using Port = Ps2Port;

    private:
        /// Flags for the notifications to the worker thread
        enum NotifyFlags: uintptr_t {
            /// Keyboard interrupt
            kKeyboardIrq                = (1 << 0),
            /// Mouse interrupt
            kMouseIrq                   = (1 << 1),
        };

        /// Commands we can send to the controller
        enum Command: uint8_t {
            /// Read controller configuration byte
            kGetConfigByte              = 0x20,
            /// Write controller configuration byte
            kSetConfigByte              = 0x60,

            /// Perform controller self-test
            kSelfTest                   = 0xAA,

            /**
             * Test port 1
             *
             * Response is 0 if test passed, an error code otherwise.
             */
            kTestPort1                  = 0xAB,
            /// Disable port 1
            kDisablePort1               = 0xAD,
            /// Enable port 1
            kEnablePort1                = 0xAE,
            /**
             * Test port 2
             *
             * Response is 0 if test passed, an error code otherwise.
             */
            kTestPort2                  = 0xA9,
            /// Disable port 2
            kDisablePort2               = 0xA7,
            /// Enable port 2
            kEnablePort2                = 0xA8,
            /// Writes the next data byte to the second PS/2 port
            kWritePort2                 = 0xD4,
        };

        /// Config byte fields
        enum Config: uint8_t {
            /// Port 1 interrupt enable
            kInterruptsPort1            = (1 << 0),
            /// Port 2 interrupt enable
            kInterruptsPort2            = (1 << 1),
            /// Port 1 clock inhibit (0 = clock enabled)
            kClockInhibitPort1          = (1 << 4),
            /// Port 2 clock inhibit (0 = normal clocking)
            kClockInhibitPort2          = (1 << 5),
            /// Port 1 legacy scancode conversion
            kScancodeConversion         = (1 << 6),
        };

        /// Status byte fields
        enum Status: uint8_t {
            /// Output buffer full (indicates data can be READ from data port)
            kOutputBufferFull           = (1 << 0),
            /// Input buffer full (indicates data is pending to be sent; you cannot write more)
            kInputBufferFull            = (1 << 1),

            /// Time-out error
            kTimeout                    = (1 << 6),
            /// Parity error
            kParity                     = (1 << 7),
        };

    public:
        Ps2Controller(const std::span<std::byte> &aux);
        ~Ps2Controller();

        /// Enters the driver's run loop
        void workerMain();

        /// Whether we are accepting device commands at this time
        constexpr inline bool isAcceptingCommands() {
            return this->acceptCommands;
        }
        /// Submits a command
        void submit(const Port, Ps2Command &);

        /// Writes a byte of data to the first or second port.
        void writeDevice(const Port port, const std::byte cmd, const int timeout = -1);

        /// Force the given device to be re-detected.
        void forceDetection(const Port);
        /// Sets the device that's connected to a particular port.
        void setDevice(const Port p, const std::shared_ptr<Ps2Device> &dev);

    private:
        /// Read a byte from the controller.
        bool readBytePoll(uint8_t &out, const int timeout = -1);
        /// Writes a controller command.
        void writeCmd(const uint8_t cmd);
        /// Writes a controller command and a single argument byte.
        void writeCmd(const uint8_t cmd, const uint8_t arg1);

        void decodeResources(mpack_node_t &node, const bool isKbd);
        void handlePortResource(const acpi_resource_io &, const size_t);

        void init();
        void deinit();
        void reset();

        /// Process any pending device commands
        bool checkDeviceCommand(const size_t i, const std::byte data);

    private:
        /// Whether resources associated with the controller are logged
        static bool gLogResources;
        /// When set, all controller commands are logged
        static bool gLogCmds;
        /// Whether reads from the controller are logged
        static bool gLogReads;
        /// When set, commands sent to devices are logged
        static bool gLogDeviceCmds;
        /// When set, reads from devices are logged
        static bool gLogDeviceReads;
        /// Log command submission and completion
        static bool gLogCommands;

    private:
        /// whether this is a dual-port controller with mouse support
        bool hasMouse = false;

        /// IRQ for the keyboard port
        uintptr_t kbdIrq = 0;
        /// IRQ for the mouse port
        uintptr_t mouseIrq = 0;

        /// access to the 8042 ports
        std::unique_ptr<PortIo> io;

        /// Address of the data port
        uint16_t dataPort = 0;
        /// Command and status port address
        uint16_t cmdPort = 0;

        /// whether we're able to accept commands
        std::atomic_bool acceptCommands{true};
        /// Run the run loop as long as this is set
        std::atomic_bool run;
        /// Native thread handle for the notification thread
        uintptr_t threadHandle = 0;

        /// Interrupt handler token for the keyboard irq
        uintptr_t kbdIrqHandler = 0;
        /// Interrupt handler token for the mouse irq
        uintptr_t mouseIrqHandler = 0;

        /// detector instances for each port
        std::array<std::unique_ptr<PortDetector>, 2> detectors;
        /// devices connected to each port (if any)
        std::array<std::shared_ptr<Ps2Device>, 2> devices;

        /// queue of commands pending to be sent to each device
        std::array<std::queue<Ps2Command>, 2> cmdQueue;
};

#endif
