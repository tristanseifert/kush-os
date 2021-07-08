#ifndef DRIVERSUPPORT_GFX_DISPLAYCLIENT_H
#define DRIVERSUPPORT_GFX_DISPLAYCLIENT_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <DriverSupport/gfx/Types.h>
#include <DriverSupport/gfx/Client_Display.hpp>

namespace DriverSupport::gfx {
/**
 * Provides a wrapper around the RPC interface of the graphics device. This exposes the underlying
 * framebuffers of a display.
 */
class Display: public rpc::DisplayClient {
    public:
        /// Name of the property on the GPU device containing connection info
        constexpr static const std::string_view kConnectionPropertyName{"display.connection"};

        using Point = std::pair<size_t, size_t>;
        using Size = std::pair<size_t, size_t>;

    public:
        enum Errors: int {
            InternalError                       = -79000,
            /// Provided device path is not valid
            InvalidPath                         = -79001,
            /// Failed to get the connection info from the provided device
            InvalidConnectionInfo               = -79002,
        };

        /// Returns the forest path from which the device was initialized.
        const auto &getForestPath() const {
            return this->forestPath;
        }

    public:
        using DisplayClient::RegionUpdated;

        [[nodiscard]] static int Alloc(const std::string_view &forestPath,
                std::shared_ptr<Display> &outPtr);
        virtual ~Display();

        /**
         * Indicates to the driver that the provided region has been updated and should be updated
         * on the display.
         */
        int32_t RegionUpdated(const Point &origin, const Size &size) {
            const auto [x, y] = origin;
            const auto [w, h] = size;
            return this->RegionUpdated(x, y, w, h);
        }

    private:
        Display(const std::string_view &path, const uint32_t gpuId,
                const std::shared_ptr<IoStream> &io);

    private:
        /// Records errors during initialization
        int status{0};
        /// Forest path of the node that owns us
        std::string forestPath;
        /// ID of the display we control
        uint32_t displayId{0};
};
}

#endif
