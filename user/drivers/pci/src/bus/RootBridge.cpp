#include "RootBridge.h"
#include "PciConfig.h"

#include "Log.h"

#include <mpack/mpack.h>

bool RootBridge::gLogIrqMap = false;

/**
 * Creates a root bridge, with binary encoded msgpack message that indicates, at a minimum, the
 * bus location of this root bridge.
 */
RootBridge::RootBridge(const std::span<std::byte> &in) {
    // set up a msgpack reader
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(in.data()), in.size());
    mpack_tree_parse(&tree);

    mpack_node_t root = mpack_tree_root(&tree);

    // get bridge location
    this->bus = mpack_node_u8(mpack_node_map_cstr(root, "bus"));
    this->segment = mpack_node_u8(mpack_node_map_cstr(root, "segment"));
    this->address = mpack_node_u32(mpack_node_map_cstr(root, "address"));

    // irq mapping if provided
    auto irqMapNode = mpack_node_map_cstr(root, "irqs");
    if(mpack_node_type(irqMapNode) == mpack_type_map) {
        // iterate over all entries in the map
        const auto mapEntries = mpack_node_map_count(irqMapNode);

        for(size_t i = 0; i < mapEntries; i++) {
            IrqInfo info;

            // key is device id
            auto keyNode = mpack_node_map_key_at(irqMapNode, i);
            auto deviceId = mpack_node_u8(keyNode);

            // value is a map containing keys 0-3 for INTA-INTD
            auto valNode = mpack_node_map_value_at(irqMapNode, i);

            auto a = mpack_node_map_int(valNode, 0);
            if(mpack_node_type(a) == mpack_type_map) {
                info.inta = Irq(a);
            }
            auto b = mpack_node_map_int(valNode, 1);
            if(mpack_node_type(b) == mpack_type_map) {
                info.intb = Irq(b);
            }
            auto c = mpack_node_map_int(valNode, 2);
            if(mpack_node_type(c) == mpack_type_map) {
                info.intc = Irq(c);
            }
            auto d = mpack_node_map_int(valNode, 3);
            if(mpack_node_type(d) == mpack_type_map) {
                info.intd = Irq(d);
            }

            // save irq info
            this->irqMap.emplace(deviceId, std::move(info));
        }
    } else {
        Trace("no irq map for bridge %p", this);
    }

    if(gLogIrqMap) {
        for(const auto &[device, info] : this->irqMap) {
            int a = -1, b = -1, c = -1, d = -1;
            if(info.inta) {
                a = info.inta->num;
            }
            if(info.intb) {
                b = info.intb->num;
            }
            if(info.intc) {
                c = info.intc->num;
            }
            if(info.intd) {
                d = info.intd->num;
            }

            Trace("device %2u: INTA %2d INTB %2d INTC %2d INTD %2d", device, a, b, c, d);
        }
    }

    // clean up
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        throw std::runtime_error("failed to decode aux data");
    }

    // discover devices
    this->scan();
}

/**
 * Deserializes an irq info struct from the given node.
 */
RootBridge::Irq::Irq(mpack_node_t &root) {
    // irq number is index 0
    this->num = mpack_node_u8(mpack_node_map_int(root, 0));
}



/**
 * Attempts to enumerate all devices connected to the given PCI bus.
 */
void RootBridge::scan() {
    auto c = PciConfig::the();

    // scan all 32 devices on this bus
    for(size_t dev = 0; dev < 32; dev++) {
        // read out vendor id; if 0xFFFF, no device here
        auto vendor = c->read(this->bus, dev, 0, 0, PciConfig::Width::Word);
        if(vendor == 0xFFFF) continue;

        // determine if it's multifunction device
        auto headerType = c->read(this->bus, dev, 0, 0xE, PciConfig::Width::Byte);
        bool isMultifunction = (headerType & 0x80);

        // check each of its functions out
        for(size_t func = 0; func < (isMultifunction ? 8 : 1); func++) {
            // read vendor id and skip the device if it's not defined
            const auto id = c->read(this->bus, dev, func, 0, PciConfig::Width::QWord);
            const auto vendor = (id & 0xFFFF), product = (id & 0xFFFF0000) >> 16;

            if(vendor == 0xFFFF) continue;

            Trace("Device %2u:%u %04x:%04x", dev, func, vendor, product);
        }
    }
}
