#ifndef ACPIUTILS_H
#define ACPIUTILS_H

#include <cstring>
#include <string>

#include <acpi.h>

#include "log.h"

/**
 * Helper functions for working with ACPI
 */
class AcpiUtils {
    public:
        /**
         * Gets the name of an ACPI object.
         */
        static std::string getName(ACPI_HANDLE object) {
            static constexpr size_t kMaxNameLen = 200;

            // set up the buffer
            ACPI_STATUS status;
            ACPI_BUFFER buffer;
            char name[kMaxNameLen];
            memset(name, 0, kMaxNameLen);

            buffer.Length = kMaxNameLen;
            buffer.Pointer = name;

            // perform request
            status = AcpiGetName(object, ACPI_FULL_PATHNAME, &buffer);
            if(ACPI_FAILURE(status)) {
                Abort("AcpiGetName failed: %s", AcpiFormatException(status));
            }

            return std::string(name);
        }
};

#endif
