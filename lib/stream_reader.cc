#include <stdio.h>
#include <cstring>

#include "multipart_stream_reader.h"

namespace xameleon
{

    void multipart_stream_reader_t::set_bound(const char* bound)
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

    const char* multipart_stream_reader_t::get_pchar(bool& out_sync, int& rest)
    {
        while (true)
        {
            if (data_length == 0) {
//                printf("Will try to load %ld bytes chunk: ", sizeof(buffer));
                const long long timeout_5_minutes = 1000000 * 60 * 5;
                const long long timeout_5_seconds = 1000000 * 5;

                data_length = fp->recv(buffer, sizeof(buffer), fp->is_secured() ? timeout_5_minutes : timeout_5_seconds);
                if (data_length < 0) {
                    out_sync = true;
                    last_state = SockerError;
                    return error_socket;
                }
                if (data_length == 0) {
                    out_sync = true;
                    last_state = Complete;
                    printf("finally loaded %d bytes\n", data_length);
                    return zero_socket;
                }
                if (rest_size == 0)
                {
                    if (data_length < (int)sizeof(buffer))
                    {
                        rest_size = data_length;
                    }

                }
//                printf("loaded %d bytes\n", data_length);
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

                    available_bytes = data_length - reader_position;

                    if (available_bytes < 4) {
                        if (buffer[reader_position + 1] != '\n')
                            fprintf(stderr, "Last two bytes? ");
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
                if (rest_size != -1)
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
}
