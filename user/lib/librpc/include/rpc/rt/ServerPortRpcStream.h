#ifndef LIBRPC_RPC_RT_SERVERPORTRPCSTREAM_H
#define LIBRPC_RPC_RT_SERVERPORTRPCSTREAM_H

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
 * Implements a server RPC stream that exchanges messages with remote clients that provide their
 * own receive ports.
 */
class ServerPortRpcStream: public ServerRpcIoStream {
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
         * Initializes a new listening stream by allocating a receive port, then announcing the
         * port under the given name.
         */
        ServerPortRpcStream(const std::string_view &service,
                const size_t _rxBufSize = kDefaultRxBufSize) : ownsReceivePort(true),
                rxBufSize(_rxBufSize) {
            int err;
            if(service.empty() || !_rxBufSize) {
                throw std::invalid_argument("Invalid target or receive buffer size");
            }

            // allocate port and register service
            err = PortCreate(&this->receivePort);
            if(err) {
                throw std::system_error(err, std::generic_category(), "PortCreate");
            }

            err = RegisterService(service.data(), this->receivePort);
            if(err) {
                throw std::system_error(err, std::generic_category(), "RegisterService");
            }

            this->commonInit(_rxBufSize);
        }

        /**
         * Initializes a new port-based RPC stream with an already allocated listening port.
         */
        ServerPortRpcStream(const uintptr_t listenPort, const size_t rxBufSize = kDefaultRxBufSize) :
            receivePort(listenPort), rxBufSize(rxBufSize) {
            if(!listenPort || !rxBufSize) {
                throw std::invalid_argument("Invalid listen port handle or receive buffer size");
            }

            this->commonInit(rxBufSize);
        }

        /**
         * Cleans up allocated memory belonging to the server port, and closes the port if it was
         * allocated by the constructor.
         */
        virtual ~ServerPortRpcStream() {
            if(this->ownsReceivePort) PortDestroy(this->receivePort);
            free(this->rxBuf);
            if(this->txBuf) free(this->txBuf);
        }

        /**
         * Receives a message from the receive port.
         *
         * @return Whether a message was received or not.
         */
        bool receive(std::span<std::byte> &outRxBuf, const bool block) override {
            int err;

            // try to receive message
            auto msg = reinterpret_cast<struct MessageHeader *>(this->rxBuf);
            err = PortReceive(this->receivePort, msg, this->rxBufSize, block ? UINTPTR_MAX : 0);

            if(!block && !err) return false;
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
            this->replyTo = packet->replyTo;

            outRxBuf = std::span(packet->payload, msg->receivedBytes - sizeof(Packet));

            return true;
        }


        /**
         * Sends a message to the client that most recently sent us a message.
         */
        bool reply(const std::span<std::byte> &buf) override {
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
            err = PortSend(this->replyTo, this->txBuf, size);
            if(err) {
                throw std::system_error(err, std::generic_category(), "PortSend");
                return false;
            }

            return true;
        }

    private:
        /**
         * Allocates the receive buffer.
         */
        void commonInit(const size_t rxBufSize) {
            int err;

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
        /// port handle we receive messages on
        uintptr_t receivePort{0};
        /// whether we need to release the port handle
        bool ownsReceivePort{false};

        /// port to send the next reply to
        uintptr_t replyTo{0};

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
