#ifndef PS2DEVICE_H
#define PS2DEVICE_H

class Ps2Device {
    public:
        /// A byte of data was received on the device's port.
        void handleRx(const uint8_t data);
};

#endif
