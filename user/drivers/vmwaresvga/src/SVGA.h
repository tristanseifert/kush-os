#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <libpci/Device.h>

namespace svga {
class Commands2D;
class FIFO;
class RpcServer;
}

class SVGA {
    friend class svga::Commands2D;
    friend class svga::FIFO;
    friend class svga::RpcServer;

    using BAR = libpci::Device::AddressResource;

    /**
     * Initial video mode to program into the device immediately after startup. This should be
     * something safe that basically everything we might run on supports; since this is virtual
     * hardware, it doesn't really matter what it is though.
     */
    constexpr static const std::tuple<uint32_t, uint32_t, uint8_t> kDefaultMode{1024, 768, 32};

    /// Name to register displays under
    constexpr static const std::string_view kDeviceName{"GenericDisplay"};

    public:
        /// Error codes specific to the SVGA driver
        enum Errors: int {
            /// The device is missing a required memory region.
            MissingBar                          = -71000,
            /// Failed to negotiate the communication version for the device
            UnsupportedVersion                  = -71001,
            /// Unsupported video mode requested
            InvalidMode                         = -71002,
            /// The provided command is too large to fit into the FIFO.
            CommandTooLarge                     = -71003,
            /// The command must be aligned
            CommandNotAligned                   = -71004,
            /// Attempted to start a new command before finishing the previous one
            CommandInFlight                     = -71005,
            /// Attempted to commit a command when there are no commands in flight
            NoCommandsAvailable                 = -71006,
        };

    public:
        /// Allocate an SVGA driver.
        static int Alloc(const std::shared_ptr<libpci::Device> &dev, std::shared_ptr<SVGA> &out);

        ~SVGA();

        /// Enable the graphics device.
        void enable();
        /// Disable the graphics device.
        void disable();

        /// Set the video mode of the device
        int setMode(const uint32_t width, const uint32_t height, const uint8_t bpp,
                const bool enable = true);

        /// Enters the device's message processing loop
        int runLoop();

        /// Get 2D command handler
        auto &get2DCommands() {
            return this->cmd2d;
        }

        /// Gets the current framebuffer size
        constexpr auto getFramebufferDimensions() const {
            return this->fbSize;
        }
        /// Gets the path under which the SVGA device is registered
        constexpr auto &getForestPath() const {
            return this->forestPath;
        }

    private:
        /// Produce logging about the device during initialization
        constexpr static const bool kLogInit{true};

    private:
        SVGA(const std::shared_ptr<libpci::Device> &dev);
        [[nodiscard]] bool mapRegisters(const BAR &);
        [[nodiscard]] bool mapVram(const BAR &);
        [[nodiscard]] bool initIrq();

        /// Reads a device configuration register
        uint32_t regRead(const size_t reg);
        /// Writes a device configuration register
        void regWrite(const size_t reg, const uint32_t value);

        /// Registers the SVGA device in the driver forest
        void registerUnder(const std::string_view &parentDevice);

    private:
        /// is the device enabled?
        std::atomic_bool enabled{false};
        /// used to signal errors during initialization
        int status{0};

        /// PCI device we're connected to
        std::shared_ptr<libpci::Device> device;
        /// SVGA device version
        uint32_t version{0};

        /// IO base address
        uint32_t ioBase{0};
        /// Capabilities supported by the device
        uint32_t caps{0};

        /// VM handle for the VRAM region
        uintptr_t vramHandle{0};
        /// Base address of the VRAM region
        void *vram{nullptr};
        /// Number of bytes in of addressable VRAM
        size_t vramSize{0};
        /// Number of bytes of VRAM reserved for the framebuffer
        size_t vramFramebufferSize{0};

        /// framebuffer size (in pixels)
        std::pair<uint32_t, uint32_t> fbSize;
        /// bits per pixel
        uint8_t fbBpp{0};
        /// Framebuffer pitch (updated when mode is set)
        size_t fbPitch{0};

        /// FIFO handler
        std::unique_ptr<svga::FIFO> fifo;
        /// 2D commands handler
        std::unique_ptr<svga::Commands2D> cmd2d;

        /// RPC server
        std::unique_ptr<svga::RpcServer> rpc;
        /// forest path of the device
        std::string forestPath;
};
