#include "../stream_writer.h"

#include <stdio.h>
#include <cstdarg>

namespace xameleon
{
    int stream_writer::sprintf(bool flush, const char* format, ...)
    {
        int slen, sslen, result = -1;
        va_list args;
        va_start(args, format);

        do {
            if (free_bytes < sizeof(buffer) - 512) {
                sslen = transport->send(buffer, sizeof(buffer) - free_bytes);
                if (sslen != sizeof(buffer) - free_bytes) break;
                free_bytes = sizeof(buffer);
            }
            slen = vsnprintf(buffer + sizeof(buffer) - free_bytes, free_bytes, format, args);
            free_bytes -= slen;
            if (flush)
            {
                sslen = transport->send(buffer, sizeof(buffer) - free_bytes);
                if (sslen != sizeof(buffer) - free_bytes) break;
                free_bytes = sizeof(buffer);
            }
            result = slen;
        } while (false);

        va_end(args);
        return result;;
    }

    int stream_writer::send(bool flush, const char* data, int size)
    {
        int sslen, result = -1;

        do {
            if (free_bytes < size) {
                sslen = transport->send(buffer, sizeof(buffer) - free_bytes);
                if (sslen != sizeof(buffer) - free_bytes) break;
                free_bytes = sizeof(buffer);
            }
            if (size) {
                memcpy(buffer + sizeof(buffer) - free_bytes, data, size);
                free_bytes -= size;
            }
            if (flush)
            {
                sslen = transport->send(buffer, sizeof(buffer) - free_bytes);
                if (sslen != sizeof(buffer) - free_bytes) break;
                free_bytes = sizeof(buffer);
            }
            result = size;
        } while (false);

        return result;;
    }


}