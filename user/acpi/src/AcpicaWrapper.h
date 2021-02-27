#ifndef ACPISRV_ACPICAWRAPPER_H
#define ACPISRV_ACPICAWRAPPER_H

#include <cstdint>

/**
 * Provides a small wrapper around the ACPICA interfaces.
 */
class AcpicaWrapper {
    friend int main(const int, const char **);

    public:

    private:
        static AcpicaWrapper *gShared;

    private:
        static void init();

        AcpicaWrapper();

        void installHandlers();
};

#endif
