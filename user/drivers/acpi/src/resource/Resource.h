#ifndef RESOURCE_RESOURCE_H
#define RESOURCE_RESOURCE_H

namespace acpi {
/*
 * Describes a resource required by a device.
 */
struct Resource {
    enum class Type {
        /**
         * A range of physical memory
         */
        Memory,
        /**
         * Defines a region of IO address space, if the platform supports it.
         */
        IoSpace,
        /**
         * Interrupt resource: described by acpi::resource::Irq struct.
         */
        Interrupt,
    };

    /// Type of the resource
    Type type;
};
}

#endif
