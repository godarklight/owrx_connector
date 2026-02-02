#pragma once

#include "iq_connection.hpp"

#include <csdr/ringbuffer.hpp>

namespace Owrx {

    class GNURadioTcpSocket: public IQSocket<float32_t> {
        public:
            GNURadioTcpSocket(float32_t port, Csdr::Ringbuffer<float32_t>* ringbuffer): IQSocket(port, ringbuffer) {};
        protected:
            void startNewConnection(int client_sock) override;
    };

    class GNURadioTcpConnection: public IQConnection<float32_t> {
        public:
            GNURadioTcpConnection(int client_sock, Csdr::RingbufferReader<float32_t>* reader): IQConnection(client_sock, reader) {};
        protected:
            void sendHeaders() override;
    };

}
