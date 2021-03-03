#include "driver/Messages.hpp"

#include <mutex>

#include <mpack/mpack.h>

using namespace libdriver;

/**
 * Serializes the message into the given byte buffer.
 *
 * @return Actual number of bytes written
 */
size_t Message::serialize(std::span<std::byte> &out) {
    // create a memory bound serializer
    mpack_writer_t writer;
    mpack_writer_init(&writer, reinterpret_cast<char *>(out.data()), out.size());

    // serialize the object
    this->serialize(&writer);

    const auto written = mpack_writer_buffer_used(&writer);

    // complete it
    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        throw std::runtime_error("mpack_writer_destroy() failed");
    }

    return written;
}

/**
 * Deserializes the message from the given buffer's top level node.
 */
void Message::deserializeFull(const std::span<std::byte> &in) {
    // read message and parse it
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(in.data()), in.size());
    mpack_tree_parse(&tree);

    // decode
    auto root = mpack_tree_root(&tree);
    this->deserialize(root);

    // clean up
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        throw std::runtime_error("mpack_tree_destroy() failed");
    }
}

const uint32_t Message::getRpcType() const {
    return 0;
}
