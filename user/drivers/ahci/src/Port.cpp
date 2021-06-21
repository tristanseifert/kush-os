#include "Port.h"
#include "Controller.h"
#include "Device.h"
#include "AtaCommands.h"
#include "AhciRegs.h"
#include "PortStructs.h"
#include "device/AtaDisk.h"

#include "Log.h"

#include "util/String.h"

#include <algorithm>
#include <cstring>

#include <driver/ScatterGatherBuffer.h>

#include <unistd.h>
#include <sys/syscalls.h>

/// Region of virtual memory space for AHCI port command list/receive FIS blobs
uintptr_t Port::kPrivateMappingRange[2] = {
    // start
    0x10018000000,
    // end
    0x10019000000,
};
/**
 * Creates a new AHCI port object for the given controller. This allocates all memory, initializes
 * it and configures the port and spins up any attached devices, as well as identifying them.
 */
Port::Port(Controller *controller, const uint8_t _port) : port(_port), parent(controller) {
    int err;
    auto &regs = controller->abar->ports[_port];

    // allocate the command list and received FIS
    const size_t pageSz = sysconf(_SC_PAGESIZE);
    size_t allocSize = kCommandTableOffset;
    allocSize += (0x80 + (kCommandTableNumPrds * sizeof(PortCommandTablePrd))) * controller->getQueueDepth();

    allocSize = ((allocSize + pageSz - 1) / pageSz) * pageSz;

    err = AllocVirtualAnonRegion(allocSize,
            (VM_REGION_RW | VM_REGION_WRITETHRU | VM_REGION_MMIO | VM_REGION_LOCKED),
            &this->privRegionVmHandle);
    if(err) {
        Abort("%s failed: %d", "AllocVirtualAnonRegion", err);
    }

    uintptr_t base{0};
    err = MapVirtualRegionRange(this->privRegionVmHandle, kPrivateMappingRange, allocSize, 0, &base);
    kPrivateMappingRange[0] += allocSize; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        Abort("%s failed: %d", "MapVirtualRegion", err);
    }

    if(kLogInit) Trace("Mapped port %lu FIS/command list at $%p ($%p'h)", _port, base,
            this->privRegionVmHandle);

    memset(reinterpret_cast<void *>(base), 0, pageSz);

    // get its physical address
    uintptr_t physAddr{0};

    err = VirtualToPhysicalAddr(&base, 1, &physAddr);
    if(err) {
        Abort("%s failed: %d", "VirtualToPhysicalAddr", err);
    }

    // program the command list base register
    const auto cmdListPhys = physAddr + kCmdListOffset;
    this->cmdList = reinterpret_cast<volatile PortCommandList *>(base + kCmdListOffset);

    regs.cmdListBaseLow = cmdListPhys & 0xFFFFFFFF;
    if(controller->supports64Bit) {
        regs.cmdListBaseHigh = cmdListPhys >> 32;
    } else {
        if(cmdListPhys >> 32) {
            Abort("Allocated %s above 4G but controller doesn't support 64-bit addressing",
                    "command list");
        }
    }

    this->initCommandTables(base);

    // program the FIS base register
    const auto rxFisPhys = physAddr + kReceivedFisOffset;
    this->receivedFis = reinterpret_cast<volatile PortReceivedFIS *>(base + kReceivedFisOffset);

    regs.fisBaseLow = rxFisPhys & 0xFFFFFFFF;
    if(controller->supports64Bit) {
        regs.fisBaseHigh = rxFisPhys >> 32;
    } else {
        if(rxFisPhys >> 32) {
            Abort("Allocated %s above 4G but controller doesn't support 64-bit addressing",
                    "received FIS buffer");
        }
    }

    if(kLogInit) Trace("Received FIS at $%p (%p), command list $%p (%p)", this->receivedFis,
            rxFisPhys, this->cmdList, cmdListPhys);

    // enable FIS reception and command processing
    this->startCommandProcessing();

    // enable port interrupts
    regs.irqEnable = AhciPortIrqs::DeviceToHostReg | AhciPortIrqs::TaskFileError |
        AhciPortIrqs::ReceiveOverflow;// | AhciPortIrqs::DescriptorProcessed;
}

/**
 * Initializes all command tables for this port. We'll store the virtual address in the command
 * tables array, and then program the command list's physical address field for the corresponding
 * command table.
 */
void Port::initCommandTables(const uintptr_t vmBase) {
    int err;
    const size_t commandTableSize{0x80 + (0x10 * kCommandTableNumPrds)};

    for(size_t i = 0; i < this->parent->getQueueDepth(); i++) {
        const size_t offset{kCommandTableOffset + (i * commandTableSize)};
        const auto address{vmBase + offset};
        if(address & 0b1111111) {
            Abort("Failed to maintain 128 byte alignment for command tables");
        }

        this->cmdTables[i] = reinterpret_cast<volatile PortCommandTable *>(address);

        auto start = reinterpret_cast<volatile std::byte *>(this->cmdTables[i]);
        std::fill(start, start+commandTableSize, std::byte{0});

        // get its physical address and program into the command headers
        uintptr_t physAddr{0};

        err = VirtualToPhysicalAddr(&address, 1, &physAddr);
        if(err) {
            Abort("%s failed: %d", "VirtualToPhysicalAddr", err);
        }

        auto &hdr = this->cmdList->commands[i];
        hdr.cmdTableBaseLow = physAddr & 0xFFFFFFFF;
        if(this->parent->is64BitCapable()) {
            hdr.cmdTableBaseHigh = physAddr >> 32;
        }
    }
}

/**
 * Cleans up resources belonging to the port.
 */
Port::~Port() {
    this->stopCommandProcessing();

    // shut down the port resources

    // unmap the VM object and delete it
    UnmapVirtualRegion(this->privRegionVmHandle);
    DeallocVirtualRegion(this->privRegionVmHandle);
}



/**
 * Starts command processing and FIS reception.
 */
void Port::startCommandProcessing() {
    auto &regs = this->parent->abar->ports[this->port];

    // wait for any current command processing to complete
    while(regs.command & AhciPortCommand::CommandEngineRunning) {};
    // enable FIS reception and command sending
    regs.command = regs.command | AhciPortCommand::ReceiveFIS;
    regs.command = regs.command | AhciPortCommand::SendCommand;
}

/**
 * Stops command processing an FIS reception.
 */
void Port::stopCommandProcessing() {
    auto &regs = this->parent->abar->ports[this->port];

    // disable command processing and FIS reception
    regs.command = regs.command & ~AhciPortCommand::SendCommand;
    regs.command = regs.command & ~AhciPortCommand::ReceiveFIS;

    // wait for FIS receive and command processing to finish
    while(1) {
        if(regs.command & AhciPortCommand::CommandEngineRunning) continue;
        if(regs.command & AhciPortCommand::ReceiveFIS) continue;

        break;
    }
}


/**
 * Probes the device attached to the port.
 */
void Port::probe() {
    this->identDevice();
}

/**
 * Identifies the attached device. We'll look at the port signature to see what kind of device is
 * connected, and send either the ATA IDENTIFY DEVICE or the ATAPI command.
 */
void Port::identDevice() {
    int err;
    auto &regs = this->parent->abar->ports[this->port];

    switch(regs.signature) {
        // Initialize a SATA disk.
        case AhciDeviceSignature::SATA: {
            std::shared_ptr<AtaDisk> disk;
            err = AtaDisk::Alloc(this->shared_from_this(), disk);
            if(err) {
                Warn("Failed to allocate %s on port %u: %d", "ATA disk", this->port, err);
            } else {
                REQUIRE(disk, "No error but failed to allocate device");
                this->portDevice = std::move(disk);
            }
            break;
        }

        case AhciDeviceSignature::SATAPI:
            Success("SATAPI device at port %u", this->port);
            this->identSatapiDevice();
            break;

        case AhciDeviceSignature::PortMultiplier:
            Warn("%s on port %u is not supported", "Port multiplier", this->port);
            break;
        case AhciDeviceSignature::EnclosureManagement:
            Warn("%s on port %u is not supported", "Enclosure management", this->port);
            break;

        // no device or is unsupported type
        default:
            break;
    }
}

/**
 * Identifies a SATAPI device attached to the port. ATAPI devices are any packed based SCSI style
 * devices, like optical drives, tape drives, and so forth.
 */
void Port::identSatapiDevice() {
    int err;

    // Issue an ATA IDENTIFY PACKET DEVICE command
    std::shared_ptr<libdriver::ScatterGatherBuffer> buf;
    err = libdriver::ScatterGatherBuffer::Alloc(512, buf);
    REQUIRE(!err, "failed to initialize DMA buffer: %d", err);

    err = this->submitAtaCommand(AtaCommand::IdentifyPacket, buf, [&, buf](const auto &res) {
        if(res.isSuccess()) {
            auto span = static_cast<std::span<std::byte>>(*buf);
            auto model = std::string(reinterpret_cast<const char *>(span.subspan(54, 40).data()), 40);
            util::ConvertAtaString(model);
            util::TrimTrailingWhitespace(model);

            auto serial = std::string(reinterpret_cast<const char *>(span.subspan(20, 20).data()), 20);
            util::ConvertAtaString(serial);
            util::TrimTrailingWhitespace(serial);

            Trace("Model '%s', serial '%s'", model.c_str(), serial.c_str());
        } else {
            Warn("%s Identify for port %u failed: status %02x", "ATAPI", this->port,
                    res.getAtaError());
        }
    });

    if(err) {
        Warn("Failed to identify %s device on port %u: %d", "SATAPI", this->port, err);
        return;
    }
}




/**
 * Handles an IRQ for this port.
 */
void Port::handleIrq() {
    auto &regs = this->parent->abar->ports[this->port];

    // get irq flags and acknowledge
    const auto is = regs.irqStatus;
    regs.irqStatus = is;
    if(kLogIrq) Trace("Port %u irq: %08x", this->port, is);

    // TODO: check for command errors
    /**
     * A task file error was raised; this means that a command we issued likely failed. We should
     * shortly receive a device-to-host register FIS as well, so there's not actually that much
     * for us to do here.
     */
    if(is & AhciPortIrqs::TaskFileError) {
        const auto &rfis = this->receivedFis->rfis;

        // find which command caused this error
        if(this->outstandingCommands) {
            const auto ci = regs.cmdIssue;
            const auto completedCmds = ~ci & this->outstandingCommands;
            const size_t slot = __builtin_ffsl(completedCmds) - 1;

            if(kLogIrq) Warn("Task file error %08x %02x (%lu)", completedCmds, rfis.status, slot);
        }
        // XXX: is there any reason for task file errors if there's no outstanding commands?
        else {
            Warn("Port %u nexpected task file error: status %02x error %02x", this->port,
                    rfis.status, rfis.error);
        }
    }
    /**
     * The device's register information has been updated. This usually indicates that a command
     * has completed.
     */
    if(is & AhciPortIrqs::DeviceToHostReg) {
        auto &rfis = this->receivedFis->rfis;

        // are there any outstanding commands?
        if(this->outstandingCommands) {
            // figure out which command(s) just completed
            const auto ci = regs.cmdIssue;
            const auto completedCmds = ~ci & this->outstandingCommands;

            // TODO: should this be a loop or is it a one off deal per irq?
            for(size_t i = 0; i < this->parent->getQueueDepth(); i++) {
                const uint32_t bit{1U << i};
                if(!(completedCmds & bit)) continue;

                // extract command information and call completion handler
                if(!(rfis.status & AtaStatus::Busy) && !(rfis.status & AtaStatus::Error) &&
                    (rfis.status & AtaStatus::Ready)) {
                    this->completeCommand(i, rfis, true);
                } else {
                    this->completeCommand(i, rfis, false);
                }
            }
        }
        // the register FIS was unsolicited. we just ignore these
        else {
            if(kLogIrq) Trace("Unsolicted register FIS: status %02x error %02x", rfis.status,
                    rfis.error);
        }
    }
    /**
     * A physical region descriptor completed transfering. We only enable interrupts for the last
     * descriptor in a chain, so this indicates all data for a command has transfered.
     */
    if(is & AhciPortIrqs::DescriptorProcessed) {
        Success("Finished descriptor");
    }
}



/**
 * Submits an ATA command (built in the provided host-to-device register FIS) to the device.
 *
 * @param fis Registers to write to the device. You're responsible for setting all registers for
 *        the given command.
 * @param result Buffer large enough to store the full response of this request
 * @param cb Callback to invoke when the command completes.
 *
 * @return 0 if the command was submitted successfully, otherwise a negative error code.
 */
int Port::submitAtaCommand(const RegHostToDevFIS &fis, const DMABufferPtr &result,
        const CommandCallback &cb) {
    // find a command slot
    const auto slotIdx = this->allocCommandSlot();
    auto table = this->cmdTables[slotIdx];

    // copy the command structure
    //fis.c = 1; // write to command register

    memcpy((void *) &table->commandFIS, &fis, sizeof(fis)); // yikes

    // set up the result buffer descriptors
    const auto numPrds = this->fillCmdTablePhysDescriptors(table, result, true);
    if(numPrds == -1) {
        return Errors::TooManyExtents;
    }

    // update the command list entry
    auto &cmdListEntry = this->cmdList->commands[slotIdx];
    cmdListEntry.commandFisLen = sizeof(fis) / 4;
    cmdListEntry.atapi = 0;
    cmdListEntry.write = 0;
    cmdListEntry.prefetchable = 0;
    cmdListEntry.prdByteCount = 0;
    cmdListEntry.clearBusy = 1;
    cmdListEntry.reset = 0;
    cmdListEntry.bist = 0;

    cmdListEntry.prdEntries = numPrds;

    if(kLogCmdHeaders) {
        auto ptr = (volatile uint32_t *) &cmdListEntry;
        Trace("Command header is %08x %08x %08x %08x", ptr[0], ptr[1], ptr[2], ptr[3]);
    }

    // lastly, submit the command (so it begins executing)
    CommandInfo i(cb);
    i.buffers.emplace_back(result);

    return this->submitCommand(slotIdx, std::move(i));
}

/**
 * Submits an ATA command to the device on this port.
 *
 * @note You should only invoke this when the device is confirmed to be an ATA device once the
 * signature is checked. ATAPI devices may not respond well to many ATA commands.
 *
 * @param cmd Command byte to write to the device
 * @param result Buffer large enough to store the full response of this request
 * @param cb Callback to invoke when the command completes.
 *
 * @return 0 if the command was submitted successfully, otherwise a negative error code.
 */
int Port::submitAtaCommand(const AtaCommand cmd, const DMABufferPtr &result,
        const CommandCallback &cb) {
    // build the register FIS
    RegHostToDevFIS fis;
    fis.command = static_cast<uint8_t>(cmd);
    fis.c = 1; // write to command register

    // then submit command
    return this->submitAtaCommand(fis, result, cb);
}

/**
 * Updates the physical region descriptors (PRDs) of the given command table so that they map to
 * the physical pages of the given scatter/gather buffer.
 *
 * @param table Command table whose PRDs are to be updated
 * @param buf Scatter/gather buffer to insert mappings for
 * @param irq If set, we'll set the PRD's "irq on completion" bit for the last one.
 *
 * @return Total number of PRDs written.
 */
size_t Port::fillCmdTablePhysDescriptors(volatile PortCommandTable *table,
    const std::shared_ptr<libdriver::ScatterGatherBuffer> &buf, const bool irq) {
    // ensure the buffer can fit in the number of PRDs we have
    const auto &extents = buf->getExtents();
    if(extents.size() > kCommandTableNumPrds) {
        return -1;
    }

    // fill a PRD for each extent
    for(size_t i = 0; i < extents.size(); i++) {
        const auto &extent = extents[i];
        auto &prd = table->descriptors[i];

        prd.numBytes = extent.getSize() - 1; // always set bit 0
        const auto phys = extent.getPhysAddress();

        prd.physAddrLow = phys & 0xFFFFFFFF;
        if(this->parent->is64BitCapable()) {
            prd.physAddrHigh = phys >> 32;
        }

        // if it's the last extent, we want an IRQ on completion
        prd.irqOnCompletion = (irq && i == (extents.size() - 1)) ? 1 : 0;

        if(kLogPrds) {
            auto ptr = (volatile uint32_t *) &prd;
            Trace("PRD is %08x %08x %08x %08x", ptr[0], ptr[1], ptr[2], ptr[3]);
        }
    }

    return extents.size();
}

/**
 * Returns the index of a command slot that is ready for use. It is marked as allocated.
 *
 * If there are currently no command slots available, we'll block on a condition variable that's
 * signaled any time a command completes and an old slot can be reused.
 */
size_t Port::allocCommandSlot() {
    /*
     * Find an empty slot, i.e. a bit in `busyCommands` that is clear. Since there's no guarantee
     * that the AHCI controller supports all 32 command slots, we can't use the fast intrinsics
     * and instead have to loop the entire array.
     */
    for(size_t i = 0; i < this->parent->getQueueDepth(); i++) {
        const uint32_t bit{(1U << i)};
        if(!(this->busyCommands & bit)) {
            this->busyCommands |= bit;
            return i;
        }
    }

    // TODO: block on some condition variable and retry
    Abort("Failed to find available command (TODO: wait for command availability)");
}

/**
 * Submits the given command. This will insert it into the outstanding commands map, then notify
 * the HBA that this command is ready to execute.
 *
 * @return 0 on success, negative error code otherwise.
 */
int Port::submitCommand(const uint8_t slot, CommandInfo info) {
    auto &regs = this->parent->abar->ports[this->port];

    // record keeping
    {
        std::lock_guard<std::mutex> lg(this->inFlightCommandsLock);
        this->outstandingCommands |= (1 << slot);
        this->inFlightCommands[slot].emplace(std::move(info));
    }

    // start command
    regs.cmdIssue = (1 << slot);
    return 0;
}

/**
 * Marks the given command as completed, whether that is with a success or a failure.
 */
void Port::completeCommand(const uint8_t slot, const volatile RegDevToHostFIS &rfis,
        const bool success) {
    std::lock_guard<std::mutex> lg(this->inFlightCommandsLock);
    const uint32_t bit{1U << slot};

    // get the command and build result structure
    auto &cmd = this->inFlightCommands[slot];
    if(!cmd) Abort("Requested completion for slot %u but no command in flight!", slot);

    CommandResult res(rfis.status);

    if(success) {
        CommandResult::Success s;
        res.storage = s;
    } else {
        CommandResult::Failure f{rfis.error};
        res.storage = f;
    }

    /*
     * Push the callback to the work queue of the controller. We'll then release the command object
     * (since we expect whoever invoked the command to still hold a reference to the appropriate
     * DMA buffers, which contain the actual data) and mark the device resources as reusable.
     *
     * We wait to actually mark the command slot as no longer busy until after the callback returns
     * so that the callback can peruse through received FISes, registers, etc.
     */
    auto callback = cmd->callback;
    this->parent->addWorkItem([callback, res, bit, this]() {
        callback(res);

        this->busyCommands &= ~bit;
    });

    cmd.reset();
    this->outstandingCommands &= ~bit;
}
