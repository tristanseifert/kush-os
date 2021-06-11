#ifndef LIBRPC_RPC_RT_CLIENTPORTRPCSTREAM_H
#define LIBRPC_RPC_RT_CLIENTPORTRPCSTREAM_H

#include <rpc/rt/RpcIoStream.hpp>
#include <rpc/dispensary.h>
#include <sys/syscalls.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace rpc::rt {
/**
 * Implements a client RPC stream that exchanges messages with a remote server over a set of ports.
 */
class ClientPortRpcStream: public ClientRpcIoStream {
    constexpr static const size_t kDefaultRxBufSize = (1024 * 16);

    /**
     * Header prepended to each message sent so that the remote end of the connection knows where
     * the reply should be sent
     */
    struct Packet {
        /// port handle the reply shall go to, or 0 if no replies expected
        uintptr_t replyTo{0};
        /// actual encoded message payload
        std::byte payload[];
    };
    static_assert(!(offsetof(Packet, payload) % sizeof(uintptr_t)),
        "packet payload is not word aligned");

    public:
        /**
         * Initializes a new port-based RPC stream, looking up the given service's name to connect
         * to.
         */
        ClientPortRpcStream(const std::string_view &service,
                const size_t _rxBufSize = kDefaultRxBufSize) : rxBufSize(_rxBufSize) {
            if(service.empty() || !_rxBufSize) {
                throw std::invalid_argument("Invalid target or receive buffer size");
            }

            int err = LookupService(service.data(), &this->targetPort);
            if(!err) {
                throw std::runtime_error("Failed to resolve service name");
            } else if(err < 0) {
                throw std::system_error(err, std::generic_category(), "LookupService");
            }

            this->commonInit(_rxBufSize);
        }

        /**
         * Initializes a new port-based RPC stream. We'll allocate a receive buffer as well as the
         * receive port at this time.
         */
        ClientPortRpcStream(const uintptr_t _target, const size_t _rxBufSize = kDefaultRxBufSize) :
            targetPort(_target), rxBufSize(_rxBufSize) {
            if(!_target || !_rxBufSize) {
                throw std::invalid_argument("Invalid target or receive buffer size");
            }

            this->commonInit(_rxBufSize);
        }
        /**
         * Releases allocated buffers and destroys the receive port.
         */
        virtual ~ClientPortRpcStream() {
            PortDestroy(this->receivePort);
            free(this->rxBuf);
            if(this->txBuf) free(this->txBuf);
        }

        /**
         * Blocks waiting for a message to arrive on the reply port.
         */
        bool receiveReply(std::span<std::byte> &outRxBuf) override {
            int err;

            // receive the message
            auto msg = reinterpret_cast<struct MessageHeader *>(this->rxBuf);
            err = PortReceive(this->receivePort, msg, this->rxBufSize, UINTPTR_MAX);
            if(err < 0) {
                throw std::system_error(err, std::generic_category(), "PortReceive");
                return false;
            }

            if(msg->receivedBytes < sizeof(Packet)) {
                throw std::runtime_error("Received message too small");
                return false;
            }

            // extract the payload
            auto packet = reinterpret_cast<Packet *>(msg->data);
            outRxBuf = std::span(packet->payload, msg->receivedBytes - sizeof(Packet));

            return true;
        }

        /**
         * Sends a message to the remote end of the connection.
         */
        bool sendRequest(const std::span<std::byte> &buf) override {
            int err;

            // allocate buffer if needed
            const auto size = sizeof(Packet) + buf.size();
            this->ensureTxBuf(size);

            // prepare the message
            auto packet = reinterpret_cast<Packet *>(this->txBuf);
            memset(packet, 0, sizeof(*packet));

            packet->replyTo = this->receivePort;

            memcpy(packet->payload, buf.data(), buf.size());

            // then send
            err = PortSend(this->targetPort, this->txBuf, size);
            if(err) {
                throw std::system_error(err, std::generic_category(), "PortSend");
                return false;
            }

            return true;
        }

    private:
        /**
         * Allocate the receive port and receive buffer.
         */
        void commonInit(const size_t rxBufSize) {
            int err;

            // allocate the receive port
            err = PortCreate(&this->receivePort);
            if(err) {
                throw std::system_error(err, std::generic_category(), "PortCreate");
            }

            // allocate the receive buffer
            err = posix_memalign(&this->rxBuf, 16, rxBufSize);
            if(err) {
                throw std::system_error(err, std::generic_category(), "posix_memalign");
            }
            memset(this->rxBuf, 0, rxBufSize);
        }

        /**
         * Ensures the transmit buffer is at least the given size.
         */
        void ensureTxBuf(const size_t bytes) {
            int err;

            if(bytes > this->txBufSize) {
                if(this->txBuf) free(this->txBuf);

                err = posix_memalign(&this->txBuf, 16, bytes);
                if(err) {
                    throw std::system_error(err, std::generic_category(), "posix_memalign");
                }
            }
        }

    private:
        /// port handle of the remote end of the connection
        uintptr_t targetPort{0};
        /// port handle we allocated for the receive port
        uintptr_t receivePort{0};

        /// message receive buffer
        void *rxBuf{nullptr};
        /// size of the receive buffer
        size_t rxBufSize{0};

        /// message transmit buffer
        void *txBuf{nullptr};
        /// size of the transmit buffer
        size_t txBufSize{0};
};
} // namespace rpc::rt

#endif
