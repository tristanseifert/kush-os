#ifndef DB_DEVICEMATCH_H
#define DB_DEVICEMATCH_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Device;

/**
 * Defines the basic interface of a device match structure. Each match implements a different
 * test method that checks if the provided device can be supported by the driver.
 */
class DeviceMatch {
    public:
        virtual ~DeviceMatch() = default;

        /**
         * Tests if the driver can support the given device. If so, its priority relative to other
         * drivers is output as well. This allows multiple drivers: imagine a generic driver that
         * allows basic feature support for a wide range of hardware, with more specific drivers
         * with higher priorities for more specific hardware.
         */
        virtual bool supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) = 0;
};

/**
 * Matches a device based on the name of the driver.
 */
class DeviceNameMatch: public DeviceMatch {
    public:
        /**
         * Creates a match object that will match if any of the device's object names are found.
         */
        DeviceNameMatch(const std::string &_name, int _priority) : name(_name), priority(_priority) {}

        bool supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) override;

    private:
        /// Name to match against anywhere in the device name list
        std::string name;
        /// Constant priority value to output
        int priority{0};
};

/**
 * Matches on a PCI device based on one or more of its class, subclass, vendor id, or product id
 * fields. These values are read from the device's configuration space when the device is initially
 * added to the forest, so we simply decode that information.
 */
class PciDeviceMatch: public DeviceMatch {
    /// Whether potential matches are logged
    constexpr static const bool kLogMatch{false};

    /// Property storing the PCI device information
    static const std::string kPciExpressInfoPropertyName;

    public:
        /// Creates an empty PCI device match.
        PciDeviceMatch() = default;
        /// Creates a PCI device match with the given conjunction flag and priority.
        PciDeviceMatch(const bool _conjunction, int _priority = 0) : conjunction(_conjunction),
            priority(_priority) {}
        /// Cleans up a PCI device match.
        ~PciDeviceMatch() = default;

        /// Checks whether this device match requires all subconditions to be satisfied.
        constexpr bool isConjunction() const {
            return this->conjunction;
        }

        /// Sets a class id that the device must match.
        void setClassId(const uint8_t _class) {
            this->classId = _class;
        }
        /// Resets the class id match.
        void resetClassId() {
            this->classId.reset();
        }
        /// Sets a subclass id that the device must match.
        void setSubclassId(const uint8_t _subclass) {
            this->subclassId = _subclass;
        }
        /// Resets the subclass id match.
        void resetSubclassId() {
            this->subclassId.reset();
        }

        /// Adds a vendor/product id match.
        void addVidPidMatch(const uint16_t vid, const std::optional<uint16_t> pid = std::nullopt,
                const std::optional<int> priority = std::nullopt);

        /// Checks whether we can match against the given device.
        bool supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) override;

    private:
        /// Defines a single vid/pid match.
        struct VidPidMatch {
            /// Vendor id is always required for a match.
            uint16_t vid{0};
            /// The product id is optional; if specified, it must match.
            std::optional<uint16_t> pid{std::nullopt};

            /// If this match applies, we may override the match's actual priority.
            std::optional<int> priority;
        };

        /**
         * When set, all conditions (that is, if set, the class id, subclass id, and at least one
         * of the vendor/product ids) must match for the match to succeed.
         */
        bool conjunction{false};

        /// Class id to match against
        std::optional<uint8_t> classId{std::nullopt};
        /// Subclass id to match against
        std::optional<uint8_t> subclassId{std::nullopt};

        /// Vid/pid matches
        std::vector<VidPidMatch> vidPid;

        /// Default priority value to use for matches.
        int priority{0};
};

#endif
