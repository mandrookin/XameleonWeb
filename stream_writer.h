#pragma once
#include "session.h"

namespace xameleon
{
    class stream_writer {
        // TODO: https://en.wikipedia.org/wiki/Chunked_transfer_encoding
        transport_i* transport;
        char buffer[16 * 1024];
        int  free_bytes;
    public:
        int sprintf(bool flush, const char* format, ...);
        int send(bool flush, const char* data, int size);
        int close_on_end() {
            return transport->close();
        }

        stream_writer(https_session_t* session) {
            transport = session->get_transport();
            free_bytes = sizeof(buffer);
        }
    };
}