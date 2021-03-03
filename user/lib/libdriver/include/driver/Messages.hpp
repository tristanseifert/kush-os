#ifndef LIBDRIVER_DRIVER_MSG_MESSAGES_HPP
#define LIBDRIVER_DRIVER_MSG_MESSAGES_HPP

#include <cstddef>
#include <span>

struct mpack_writer_t;
struct mpack_node_t;

namespace libdriver {
/**
 * Base type for all messages sent to/from drivers. This exposes an interface to (de)serialize the
 * messages from/to memory buffers.
 */
struct Message {
    public:
        virtual ~Message() = default;

        /**
         * Serializes the message into the given fixed-size buffer.
         */
        virtual size_t serialize(std::span<std::byte> &out);

        /**
         * Serializes the message into the given msgbuf writer.
         */
        virtual void serialize(mpack_writer_t * _Nonnull writer) = 0;

        /**
         * Decodes the given memory buffer as the structure, MsgPack encoded, and replaces the
         * current object's contents with it.
         */
        virtual void deserializeFull(const std::span<std::byte> &in);
        /**
         * Decodes the object from a given MsgPack node.
         */
        virtual void deserialize(mpack_node_t &root) = 0;

        /**
         * Returns the RPC type value for this message.
         */
        virtual const uint32_t getRpcType() const;
};

/**
 * Sends the given message to the driver server.
 */
void SendToSrv(Message * _Nonnull msg, const uintptr_t replyPort = 0);
}

#endif
