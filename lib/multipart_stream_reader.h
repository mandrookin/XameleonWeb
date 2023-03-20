#pragma once

#include "../transport.h"

namespace xameleon
{
    class multipart_stream_reader_t {
    public:
        // Алгоритм не отработает, если размер буфера меньше 256 байт.
        static const int BUFSIZE = 16 * 1024; // 256; // 512; // 
        typedef enum { Ok, Bound, SockerError, Complete, Closed } last_state_t;
    private:
        last_state_t last_state;
        int rest_size;
        int bound_size;
        bool keep_sync;
        int data_length;
        int reader_position;
        transport_t* fp;
        char buffer[BUFSIZE];
    public:
        char bound_buffer[96];
        char* bound;
        const char* error_socket = "Socket error";
        const char* zero_socket = "Zero bytes read";
    public:
        last_state_t get_state(int& bound_size) { bound_size = this->bound_size;  return last_state; };
        transport_t* get_transport() { return fp; }
        int get_rest() { return rest_size; }

        const char* get_pchar(bool& out_sync, int& rest);

        multipart_stream_reader_t(transport_t* fp)
        {
            last_state = Ok;
            rest_size = 0; // -1;
            this->fp = fp;
            bound_size = 0;
            data_length = 0;
            reader_position = 0;
            bound = nullptr;
        }

        void set_content_length(int rest_size)
        {
            this->rest_size = rest_size;
        }

        void set_bound(const char* bound);
    };

}
