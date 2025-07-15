#pragma once

#include "iq_connection.hpp"

#include <csdr/ringbuffer.hpp>

namespace Owrx {

    class MiriTcpSocket: public IQSocket<int16_t> {
        public:
            MiriTcpSocket(int16_t port, Csdr::Ringbuffer<int16_t>* ringbuffer): IQSocket(port, ringbuffer) {};
        protected:
            void startNewConnection(int client_sock) override;
    };

    class MiriTcpConnection: public IQConnection<int16_t> {
        public:
            MiriTcpConnection(int client_sock, Csdr::RingbufferReader<int16_t>* reader): IQConnection(client_sock, reader) {};
        protected:
            void sendHeaders() override;
    };

}
