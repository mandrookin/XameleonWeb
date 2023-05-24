#pragma once

#include "../transport.h"

namespace xameleon
{
    class http_stream_t  
    {
    protected:
        transport_i* fp;
        long long receiver_timer;
    public:
        char* bound;
        const char* error_socket = "Socket error";
        const char* zero_socket = "Zero bytes read";
        virtual const char* get_pchar(bool& out_sync, int& rest) = 0;
        virtual void set_bound(const char* bound);
        http_stream_t(transport_i* fp)
        {
            this->fp = fp;
            bound = nullptr;
        }
    };

    class raw_http_stream_t : public http_stream_t {
        static const int BUFSIZE = 16 * 1024; // 256; // 512; // 
        int data_length;
        int reader_position;
        char buffer[BUFSIZE + 1024];

        const char* get_pchar(bool& out_sync, int& rest);
    public:
        raw_http_stream_t(transport_i * transport) : http_stream_t(transport) {
            data_length = 0;
            reader_position = 0;

            const long long timeout_5_minutes = 1000000 * 60 * 5;
            const long long timeout_5_seconds = 1000000 * 5;

            if (fp->is_secured())
                this->receiver_timer = timeout_5_minutes;
            else
                this->receiver_timer = timeout_5_minutes; // timeout_5_seconds; //
        }
    };

    class multipart_stream_reader_t : public http_stream_t {
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
        char buffer[BUFSIZE];
    public:
        char bound_buffer[96];
    public:
        last_state_t get_state(int& bound_size) { bound_size = this->bound_size;  return last_state; };
        transport_i* get_transport() { return fp; }
        int get_rest() { return rest_size; }
        void set_receive_timer(long long rcv_timeout) { receiver_timer = rcv_timeout; }

        const char* get_pchar(bool& out_sync, int& rest);

        multipart_stream_reader_t(transport_i* fp);

        void reset_buffer()
        {
            data_length = 0;
            reader_position = 0;
        }

        void set_content_length(int rest_size)
        {
            this->rest_size = rest_size;
        }

        void set_bound(const char* bound);
    };

}
