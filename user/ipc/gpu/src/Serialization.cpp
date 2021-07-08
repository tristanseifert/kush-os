#include "Helpers.h"

#include <DriverSupport/gfx/Types.h>

#include <cstdio>
#include <cstring>

#include <mpack/mpack.h>

using namespace DriverSupport::gfx;

/**
 * Serializes a display mode object.
 */
bool rpc::serialize(std::span<std::byte> &buf, const DriverSupport::gfx::DisplayMode &dm) {
    const auto [x, y] = dm.resolution;

    memcpy(buf.data()     , &dm.refresh, sizeof(dm.refresh));
    memcpy(buf.data() +  4, &x, sizeof(x));
    memcpy(buf.data() +  8, &y, sizeof(y));
    memcpy(buf.data() + 12, &dm.bpp, sizeof(dm.bpp));

    return true;
}

/**
 * Deserializes a display mode object.
 */
bool rpc::deserialize(const std::span<std::byte> &buf, DriverSupport::gfx::DisplayMode &dm) {
    uint32_t x, y;
    memcpy(&dm.refresh, buf.data(), sizeof(dm.refresh));
    memcpy(&x, buf.data() + 4, sizeof(x));
    memcpy(&y, buf.data() + 8, sizeof(y));
    memcpy(&dm.bpp, buf.data() + 12, sizeof(dm.bpp));

    dm.resolution = {x, y};

    return true;
}


/**
 * Number of bytes required to serialize a display mode object. This is the sum of all the fields.
 */
size_t rpc::bytesFor(const DriverSupport::gfx::DisplayMode &dm) {
    return sizeof(float) + sizeof(uint32_t)*2 + sizeof(dm.bpp);
}



/**
 * Decodes the connection information for a GPU.
 *
 * @param d Buffer containing encoded connection info blob
 * @return Valid pair of port/display id. Port is never 0
 */
std::tuple<uintptr_t, uint32_t> DriverSupport::gfx::DecodeConnectionInfo(const std::span<std::byte> &d) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(d.data()), d.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    // get the values out of it
    const auto handle = mpack_node_u64(mpack_node_map_cstr(root, "port"));
    const auto id = mpack_node_u64(mpack_node_map_cstr(root, "id"));

    // clean up
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        fprintf(stderr, "[%s] %s failed: %d\n", __PRETTY_FUNCTION__, "mpack_tree_destroy", status);
    }

    return {handle, id};
}

/**
 * Encodes the connection info for a GPU into the provided vector.
 *
 * @param port Port handle to send RPC messages to
 * @param displayId Identifier for this display
 * @param out Buffer to hold the encoded data
 *
 * @return Whether encoding succeeded
 */
bool DriverSupport::gfx::EncodeConnectionInfo(const uintptr_t port, const uint32_t displayId,
        std::vector<std::byte> &out) {
    char *data;
    size_t size;

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 2);

    // write out the size of the disk
    mpack_write_cstr(&writer, "port");
    mpack_write_u64(&writer, port);
    mpack_write_cstr(&writer, "id");
    mpack_write_u32(&writer, displayId);

    // copy to output buffer
    mpack_finish_map(&writer);

    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        fprintf(stderr, "[%s] %s failed: %d", __PRETTY_FUNCTION__, "mpack_writer_destroy", status);
        return false;
    }

    out.resize(size);
    out.assign(reinterpret_cast<std::byte *>(data), reinterpret_cast<std::byte *>(data + size));
    free(data);

    return true;
}
