#include "Client.h"
#include "Helpers.h"

#include <mpack/mpack.h>

using namespace libdriver::pci;
using namespace libdriver::pci::internal;


/**
 * Decodes the auxiliary information structure (under the `pcie.info` property on a device) to
 * get the bus address from it.
 *
 * @param data Property value to decode
 * @param outAddr Address structure to populate
 *
 * @return Whether the decoded address is valid or not.
 */
bool internal::DecodeAddressInfo(const std::span<std::byte> &data, BusAddress &outAddr) {
    bool success{false};

    // deserialize the messagepack blob
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(data.data()), data.size());
    mpack_tree_parse(&tree);

    mpack_node_t root = mpack_tree_root(&tree);

    // extract info from the reader
    outAddr.segment = mpack_node_u16(mpack_node_map_cstr(root, "segment"));
    outAddr.bus = mpack_node_u8(mpack_node_map_cstr(root, "bus"));
    outAddr.device = mpack_node_u8(mpack_node_map_cstr(root, "device"));
    outAddr.function = mpack_node_u8(mpack_node_map_cstr(root, "function"));

    success = true;

    // clean up
    mpack_tree_destroy(&tree);
    return success;
}

