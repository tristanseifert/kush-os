#include "Ps2Controller.h"
#include "Ps2Device.h"
#include "PortDetector.h"
#include "Log.h"

#include <thread>

#include <Deserialize.h>

#include <mpack/mpack.h>

#include <sys/syscalls.h>
#if defined(__i386__)
#include <sys/x86/syscalls.h>
#include <x86_io.h>
#endif

bool Ps2Controller::gLogResources       = false;
bool Ps2Controller::gLogCmds            = false;
bool Ps2Controller::gLogReads           = false;
bool Ps2Controller::gLogDeviceCmds      = false;
bool Ps2Controller::gLogDeviceReads     = false;
bool Ps2Controller::gLogCommands        = false;

/**
 * Initializes a PS/2 controller with the provided aux data structure. This should be a map, with
 * the "kbd" and "mouse" keys; each key may either be nil (if that port is not supported) or a
 * list of resources that are assigned to it.
 *
 * The kbd port lists the IO resources for the controller.
 */
Ps2Controller::Ps2Controller(const std::span<std::byte> &aux) {
    // set up a reader
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(aux.data()), aux.size());
    mpack_tree_parse(&tree);

    mpack_node_t root = mpack_tree_root(&tree);

    // have we kbd info?
    auto kbdNode = mpack_node_map_cstr(root, "kbd");
    if(mpack_node_type(kbdNode) != mpack_type_array) {
        Abort("no kbd info");
    }

    this->decodeResources(kbdNode, true);

    // get mouse info as well
    auto mouseNode = mpack_node_map_cstr(root, "mouse");
    if(mpack_node_type(mouseNode) == mpack_type_array) {
        this->decodeResources(mouseNode, false);
        this->hasMouse = true;
    } else {
        Warn("No mouse port for PS/2 controller");
    }

    // set up IO port
    this->io = std::make_unique<PortIo>(this->cmdPort, this->dataPort);

    // initialize the run loop
    this->run = true;
}

/**
 * Tears down PS/2 controller resources.
 */
Ps2Controller::~Ps2Controller() {
    // beyond this point it's no longer possible to send new device commands
    this->acceptCommands = false;
}

/**
 * Decodes the resources in the given array.
 */
void Ps2Controller::decodeResources(mpack_node_t &list, const bool isKbd) {
    ACPI_RESOURCE rsrc;
    size_t io = 0;

    const size_t nResources = mpack_node_array_length(list);

    // decode each resource
    for(size_t i = 0; i < nResources; i++) {
        auto child = mpack_node_array_at(list, i);
        acpi::deserialize(child, rsrc);

        switch(rsrc.Type) {
            case ACPI_RESOURCE_TYPE_IRQ: {
                const auto &irq = rsrc.Data.Irq;
                if(isKbd) {
                    this->kbdIrq = irq.Interrupts[0];
                } else {
                    this->mouseIrq = irq.Interrupts[0];
                }
                break;
            }

            /**
             * Assume the first IO port is the data port, and the second one is the command/status
             * port. This doesn't seem to be documented anywhere whether this is how it's supposed
             * to work.
             */
            case ACPI_RESOURCE_TYPE_IO:
                this->handlePortResource(rsrc.Data.Io, io++);
                break;

            default:
                Abort("Unsupported ACPI resource type %u", rsrc.Type);
        }
    }
}

/**
 * Handles an IO port resource.
 *
 * @param num Number of IO resource; the first one is assumed to be the data port, and the second
 * one is to be the command/status port.
 */
void Ps2Controller::handlePortResource(const ACPI_RESOURCE_IO &io, const size_t num) {
    // figure out which port it is
    if(!num) {
        this->dataPort = io.Minimum;
    } else {
        this->cmdPort = io.Minimum;
    }

    if(gLogResources) {
        Trace("IO: %04x - %04x (align %u addr len %u decode %u)", io.Minimum, io.Maximum,
                io.Alignment, io.AddressLength, io.IoDecode);
    }
}



/**
 * Worker loop for the PS/2 controller driver. This handles receiving interrupts from the device,
 * and handles commands to be sent to the controller.
 */
void Ps2Controller::workerMain() {
    int err;
    uintptr_t note = 0;

    // set up the thread
    ThreadSetName(0, "run loop");
    err = ThreadGetHandle(&this->threadHandle);
    if(err) {
        Abort("%s failed: %d", "ThreadGetHandle", err);
    }

    // set a high priority
    err = ThreadSetPriority(this->threadHandle, 80);
    if(err) {
        Abort("%s failed: %d", "ThreadSetPriority", err);
    }

    // create the controllers
    this->detectors[0] = std::make_unique<PortDetector>(this, Port::Primary);
    if(this->hasMouse) {
        this->detectors[1] = std::make_unique<PortDetector>(this, Port::Secondary);
    }

    // initialize the controller
    this->init();
    Success("work loop start");

    // work loop
    while(this->run) {
        std::byte temp;

        // wait on notification
        note = NotificationReceive(UINTPTR_MAX, UINTPTR_MAX);
        //Trace("Notify $%08x", note);

        this->inCmdLoop = true;

        // read port 1 byte
        if(note & kKeyboardIrq) {
            temp = std::byte(this->io->read(PortIo::Port::Data));

            // handle any pending commands
            if(gLogDeviceReads) Trace("<< %u %02x", 1, temp);

            if(!this->checkDeviceCommand(0, temp)) {
                // if no commands pending, forward to device or port detector
                if(this->devices[0]) {
                    this->devices[0]->handleRx(temp);
                } else {
                    this->detectors[0]->handleRx(temp);
                }
            }
        }
        // read port 2 byte
        if(note & kMouseIrq) {
            temp = std::byte(this->io->read(PortIo::Port::Data));

            // send to port 2 device, if allocated
            if(gLogDeviceReads) Trace("<< %u %02x", 2, temp);

            if(!this->checkDeviceCommand(1, temp)) {
                // if no commands pending, forward to device or port detector
                if(this->devices[1]) {
                    this->devices[1]->handleRx(temp);
                } else {
                    this->detectors[1]->handleRx(temp);
                }
            }
        }

        this->inCmdLoop = false;

        // process any commands we haven't sent yet
        if(this->testPendingCommands[0]) {
            this->cmdQueue[0].front()->send(Port::Primary, this);
            this->testPendingCommands[0] = false;
        } else if(this->testPendingCommands[1]) {
            this->cmdQueue[1].front()->send(Port::Secondary, this);
            this->testPendingCommands[1] = false;
        }
    }

    // clean up
    this->deinit();
}

/**
 * Adds a command to the device's command queue. If there are no commands pending, we start to send
 * it immediately; otherwise, the command will be transmitted after all other pending ones have
 * been completed.
 */
void Ps2Controller::submit(const Port p, const std::shared_ptr<Ps2Command> &_cmd) {
    const size_t i = (p == Port::Primary) ? 0 : 1;
    auto &cq = this->cmdQueue[i];

    const bool sendNow = cq.empty();

    // enqueue the command
    cq.push(_cmd);

    auto &cmd = cq.back();
    cmd->markPending();

    if(gLogCommands) {
        Trace("%s command $%p - %s ($%02x)", "Submitted", cmd.get(),
                (p == Port::Primary) ? "primary" : "secondary", cmd->command);
    }

    // send it immediately if we've no other pending commands
    if(sendNow) {
        if(!this->inCmdLoop) {
            cmd->send(p, this);
        } else {
            this->testPendingCommands[i] = true;
        }
    }
}

/**
 * Checks whether there's a pending command for the given device index; if so, it will be sent the
 * received byte, and the command completed if it expects no more data.
 *
 * @return Whether the data byte was handled by a pending command
 */
bool Ps2Controller::checkDeviceCommand(const size_t i, const std::byte data) {
    auto &cq = this->cmdQueue[i];
    const auto port = !i ? Port::Primary : Port::Secondary;

    // process in command
    if(!cq.empty()) {
        auto &cmd = cq.front();

        // the command is complete!
        if(cmd->handleRx(port, this, data)) {
            if(gLogCommands) {
                Trace("%s command $%p - %s ($%02x)", "Completed", cmd.get(),
                        (port == Port::Primary) ? "primary" : "secondary", cmd->command);
            }

            cq.pop();
        } else {
            return true;
        }

        // if we've any further commands, we should send it
        if(!cq.empty()) {
            auto &cmd = cq.front();
            cmd->send(port, this);
        }

        return true;
    }

    // we haven't handled the byte via a command
    return false;
}

/**
 * Forces the given device to be re-initialized. This will deallocate any existing device instance,
 * then send the a reset command to the port and go through the standard device detection
 * machinery.
 */
void Ps2Controller::forceDetection(const Port p) {
    // destroy device
    this->devices[(p == Port::Primary) ? 0 : 1].reset();

    // build the command and send it
    auto cmd = Ps2Command::Reset([](const Port p, auto command, void *ctx) {
        auto controller = reinterpret_cast<Ps2Controller *>(ctx);
        if(command->didCompleteSuccessfully()) {
            controller->detectors[(p == Port::Primary) ? 0 : 1]->reset();
        } else {
            Warn("Failed to reset device on %s port", (p == Port::Primary) ? "primary" : "secondary");
        }
    }, this);

    this->submit(p, cmd);
}

/**
 * Sets the device connected to the given port.
 */
void Ps2Controller::setDevice(const Port p, const std::shared_ptr<Ps2Device> &device) {
    const size_t i = (p == Port::Primary) ? 0 : 1;

    // store device
    this->devices[i] = device;

    // TODO: then register in device tree
}

/**
 * On completion of the detection process for the first port, kick off detection for the second
 * port if necessary.
 */
void Ps2Controller::detectionCompleted(const Port p, const bool success) {
    if(p == Port::Primary && this->detectPort2) {
        this->detectPort2 = false;
        this->forceDetection(Port::Secondary);
    }
}



/**
 * Initializes the controller driver. This registers the interrupts and performs a controller
 * reset.
 */
void Ps2Controller::init() {
    int err;

    // register interrupts
    if(this->kbdIrq) {
        err = IrqHandlerInstall(this->kbdIrq, this->threadHandle, kKeyboardIrq,
                &this->kbdIrqHandler);
        if(err) {
            Abort("failed to install %s irq: %d", "keyboard", err);
        }
    }
    if(this->mouseIrq) {
        err = IrqHandlerInstall(this->mouseIrq, this->threadHandle, kMouseIrq,
                &this->kbdIrqHandler);
        if(err) {
            Abort("failed to install %s irq: %d", "mouse", err);
        }
    }

    // then, reset the controller
    this->reset();
}

/**
 * Cleans up the PS/2 controller state. in that we disable scanning on all attached devices and
 * remove the interrupt handlers.
 */
void Ps2Controller::deinit() {
    // deallocate devices
    for(auto &device : this->devices) {
        device.reset();
    }

    this->acceptCommands = false;

    // remove the IRQ handlers
    if(this->kbdIrqHandler) {
        IrqHandlerRemove(this->kbdIrqHandler);
    }
    if(this->mouseIrqHandler) {
        IrqHandlerRemove(this->mouseIrqHandler);
    }

    // reset controller
    this->reset();
}

/**
 * Performs a controller reset.
 */
void Ps2Controller::reset() {
    uint8_t cfg, temp;

    // disable device ports and flush the output buffer
    this->writeCmd(kDisablePort1);
    this->writeCmd(kDisablePort2);

    for(size_t i = 0; i < 3; i++) {
        this->io->read(PortIo::Port::Data);
    }

    /*
     * Update the configuration byte of the controller; we disable translation, and disable the
     * interrupts for both channels for now.
     *
     * We enable clocks for both channels (if the second channel exists) at this time.
     */
    this->writeCmd(kGetConfigByte);
    if(!this->readBytePoll(cfg)) {
        Abort("failed to read %s reply", "get config byte");
    }

    cfg &= ~(kClockInhibitPort1 | kInterruptsPort2 | kInterruptsPort1 | kScancodeConversion);
    if(this->hasMouse) {
        cfg &= ~kClockInhibitPort2;
    }

    this->writeCmd(kSetConfigByte, cfg);

    /*
     * Perform a controller self-test. We expect the controller replies with 0x55 to indicate that
     * the self-test passed; any other value (0xFC is specified) is a failure.
     */
    this->writeCmd(kSelfTest);
    if(!this->readBytePoll(temp)) {
        Abort("failed to read %s reply", "self test");
    }

    if(temp != 0x55) {
        Abort("self test failed: %02x", temp);
    }

    /*
     * Test both the keyboard and mouse ports. If either of them fail, we abort. Technically this
     * could be recovered from though. The ports that work are enabled.
     */
    this->writeCmd(kTestPort1);
    if(!this->readBytePoll(temp)) {
        Abort("failed to read %s reply", "port 1 test");
    }
    if(temp) {
        Abort("Port %u failed self test: %02x", 1, temp);
    }

    this->writeCmd(kEnablePort1);

    if(this->hasMouse) {
        this->writeCmd(kTestPort2);
        if(!this->readBytePoll(temp)) {
            Abort("failed to read %s reply", "port 2 test");
        }
        if(temp) {
            Abort("Port %u failed self test: %02x", 2, temp);
        }

        this->writeCmd(kEnablePort2);
    }

    /*
     * Enable receive interrupts for all devices, then send each device a reset command. When each
     * device responds to the reset command (with an ack and the self-test response code) the
     * standard device detection logic will run in the interrupt handler (when it sees that the
     * port has no device set up yet) and probe the device.
     */
    // turn on interrupts
    this->io->write(PortIo::Port::Command, kGetConfigByte);
    if(!this->readBytePoll(temp)) {
        Abort("failed to read %s reply", "get config byte");
    }
    temp |= kInterruptsPort1;
    if(this->hasMouse) {
        temp |= kInterruptsPort2;
    }

    this->writeCmd(kSetConfigByte, temp);

    // reset both devices
    this->acceptCommands = true;
    /*this->writeDevice(Port::Primary, std::byte(0xFF));

    if(this->hasMouse) {
        this->writeDevice(Port::Secondary, std::byte(0xFF));
    }*/
    this->forceDetection(Port::Primary);
    this->detectPort2 = this->hasMouse;
}

/**
 * Reads a byte from the controller in polling mode.
 *
 * @param out If a byte is available, it is placed in this variable.
 * @param timeout How long (in microsec) to wait, or -1 to wait forever.
 * @return Whether a byte was successfully read.
 *
 * TODO: implement timeout
 */
bool Ps2Controller::readBytePoll(uint8_t &out, const int timeout) {
    uint8_t temp;

    // read data
    do {
        // check the status port
        temp = this->io->read(PortIo::Port::Command);
        if(temp & kOutputBufferFull) {
            out = this->io->read(PortIo::Port::Data);

            if(gLogReads) {
                Trace("<- %02x", out);
            }

            return true;
        }
    } while(timeout == -1);

    // if we get here, no byte was read
    return false;
}

/**
 * Writes a single byte command that requires no arguments.
 */
void Ps2Controller::writeCmd(const uint8_t cmd) {
    this->io->write(PortIo::Port::Command, cmd);

    if(gLogCmds) {
        Trace("-> %02x", cmd);
    }
}

/**
 * Writes a command with one parameter byte to the controller.
 */
void Ps2Controller::writeCmd(const uint8_t cmd, const uint8_t arg1) {
    uint8_t temp;

    // send command
    this->io->write(PortIo::Port::Command, cmd);

    // wait to be ready to accept data
    while(1) {
        temp = this->io->read(PortIo::Port::Command);
        if((temp & kInputBufferFull) == 0) {
            break;
        }
    }

    // send data
    this->io->write(PortIo::Port::Data, arg1);

    if(gLogCmds) {
        Trace("-> %02x %02x", cmd, arg1);
    }
}



/**
 * Writes a byte to the specified device.
 */
void Ps2Controller::writeDevice(const Port port, const std::byte cmd, const int timeout) {
    uint8_t temp;

    /*
     * Send a command to the controller, if needed, to set it up to send data to the port that
     * we were requested.
     *
     * For the first port, we have to do nothing; the second port requires we send a command. After
     * this, in both cases, we need to wait for the "input buffer full" flag to be cleared before
     * we can write the data to be sent to the device.
     */
    switch(port) {
        case Port::Primary:
            break;
        case Port::Secondary:
            this->io->write(PortIo::Port::Command, kWritePort2);
            break;
    }

    // wait for the input buffer to be available (TODO; timeout)
    while(1) { // TODO: timeout
        temp = this->io->read(PortIo::Port::Command);
        if((temp & kInputBufferFull) == 0) {
            break;
        }
    }

    // then write the data byte
    this->io->write(PortIo::Port::Data, static_cast<uint8_t>(cmd));

    if(gLogDeviceCmds) {
        Trace(">> %u %02x", (port == Port::Primary) ? 1 : 2, cmd);
    }
}
