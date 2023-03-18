#pragma once

#include <stdio.h>
#include <cstring>
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
        const char* get_pchar(bool& out_sync, int& rest)
        {
            while (true)
            {
                //if (rest_size == 0) {
                //    printf("Looks like we fetched all data from socket\n");
                //    out_sync = true;
                //    last_state = Complete;
                //    return zero_socket;
                //}
                if (data_length == 0) {
                    printf("Will try to load %ld bytes chunk of %ld rest\n", sizeof(buffer), rest);
                    data_length = fp->recv(buffer, sizeof(buffer));
                    if (data_length < 0) {
                        out_sync = true;
                        last_state = SockerError;
                        return error_socket;
                    }
                    if (data_length == 0) {
                        out_sync = true;
                        last_state = Complete;
                        return zero_socket;
                    }
                    if (rest_size == 0)
                    {
                        if (data_length < (int) sizeof(buffer))
                        {
                            rest_size = data_length;
                        }

                    }

                    printf("loaded %d bytes and %d bytes more %d\n", data_length, rest, rest_size);
                }

                int available_bytes = data_length - reader_position;

                bool skip = rest_size == available_bytes;

                if (!skip && available_bytes < bound_size + 4 + 2)
                {
                    for (int i = 0; i < available_bytes; i++) {
                        buffer[i] = buffer[reader_position + i];
                    }
                    int buff_size = sizeof(buffer) - available_bytes;
                    printf("Will try to load %d bytes on bound arrangement\n", buff_size);
                    buff_size = fp->recv(buffer + available_bytes, buff_size);
                    data_length = available_bytes + buff_size;
                    printf("Read %d bytes. Total data %d bytes. Rest %d bytes\n", buff_size, data_length, rest_size);
                    reader_position = 0;
                }

                if (reader_position < data_length) {
                    if (buffer[reader_position] == '\r') {

//                        int available_bytes = data_length - reader_position;

                        if (available_bytes < 4) {
                            fprintf(stderr, "Last two bytes?");
//                            throw "Somethong goes wrong. Fix me ASAP";
                            goto not_match;
                        }

                        bool has_prefix =
                            (buffer[reader_position + 1] == '\n') &&
                            (buffer[reader_position + 2] == '-') &&
                            (buffer[reader_position + 3] == '-');

                        if (!has_prefix)
                            goto not_match;

                        available_bytes -= 4;

                        if (available_bytes < bound_size)
                        {
                            throw "Fix me ASAP!!!";
                        }
                        if (bound)
                        {
#ifdef _WIN32
                            char* found = buffer + reader_position + 4;;
#endif
                            if (std::strncmp(bound, buffer + reader_position + 4, bound_size) == 0) {
                                out_sync = true;
                                last_state = Bound;
                                rest_size -= bound_size + 4;
                                reader_position += bound_size + 4;
                                return bound;
                            }
                        }
                    }
                not_match:
                    out_sync = false;
                    last_state = Ok;
                    if(rest_size != -1)
                        rest_size--;
                    rest = rest_size;
                    return buffer + reader_position++;
                }
                else
                {
                    data_length = 0;
                    reader_position = 0;
                }
            }
        }

        multipart_stream_reader_t(transport_t* fp, const char* bound)
        {
            this->last_state = Ok;
            this->rest_size = 0; // -1;;
            this->fp = fp;
            bound_size = 0;
            data_length = 0;
            reader_position = 0;
            if (bound != nullptr)
            {
                do { this->bound_buffer[bound_size] = bound[bound_size]; } while (bound[bound_size++]);
                bound_size--;
                this->bound = bound_buffer;
            }
        }

        void set_content_length(int rest_size)
        {
            this->rest_size = rest_size;
        }

        void set_bound(const char* bound)
        {
            bound_size = 0;
            this->bound = nullptr;
            if (bound != nullptr)
            {
                do { this->bound_buffer[bound_size] = bound[bound_size]; } while (bound[bound_size++]);
                bound_size--;
                this->bound = bound_buffer;
            }
        }

        char* gets(int size)
        {
            if (data_length - reader_position < size)
                return nullptr;

            return buffer + reader_position;
        }
    };

}
