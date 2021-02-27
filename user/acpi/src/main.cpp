#include <thread>

#include "AcpicaWrapper.h"

#include "log.h"

int main(const int argc, const char **argv) {
    // initialise ACPICA
    AcpicaWrapper::init();

    // enter main message loop
    Trace("Entering message loop");
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
