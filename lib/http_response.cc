#define _CRT_SECURE_NO_WARNINGS 1

#include "../session.h"
#include <stdio.h>

using namespace std;

namespace xameleon
{
    const char* http_response_t::parse_header_option(const char* name, const char* value)
    {
        return name;
    }

    const char* http_response_t::parse_header(http_stream_t* reader)
    {
        typedef enum hop { header, option, cr } header_state_t;
        bool sync = false;
        int rest;
        const char* src = nullptr;
        header_state_t state = header;

        _header_size = 0;
        while (!sync)
        {
            char  headline[4096];
            char* ptr = headline;
            //            string name;
            //            string value;
            const char* pcolon = nullptr;
            do {
                src = reader->get_pchar(sync, rest);
                _header_size++;
                if (sync) {

                    break;
                }
                if (*src == '\r')
                    *ptr++ = 0;
                if (pcolon)
                    *ptr++ = *src;
                else if (*src != ':')
                    *ptr++ = *src | 0b00100000;
                else
                {
                    *ptr++ = 0;
                    pcolon = ptr;
                }
            } while (*src != '\n');

            int linelen = ptr - headline;

            if (sync) {
                if (src == reader->zero_socket) {
                    continue;
                }
                else if (src == reader->error_socket)
                    continue;
                else if (src == reader->bound)
                    printf("Got multipart bound in response\n");
                else
                    printf("Lost sync on parsing http_response_t::parse_response\n");
            }

            switch (state)
            {
            case header:
            {
                uint32_t signature = ntohl(*((uint32_t*)headline));
                if (signature != 0x68747470) {
                    fprintf(stderr, "Non HTTP header\n");
                    return nullptr;
                }
                url_t url;

                if (headline[4] != '/') {
                    fprintf(stderr, "Broken HTTP signture");
                }
                url.version.major = headline[5] - '0';
                url.version.minor = headline[7] - '0';

                int na = sscanf(headline + 9, "%u", &this->_code);
                if (na != 1 || _code == 0) {
                    fprintf(stderr, "Broken http response - zero HTTP code\n");
                }

                string http_reason = (linelen > 13) ? string(headline + 13) : "";
                _http_reason = http_reason;
                state = option;
                continue;
            }

            case option:
                if (pcolon) {
                    if (*pcolon == ' ') pcolon++;
                    switch (hash(headline))
                    {
                    case hash("server"):
                        _server = string(pcolon);
                        break;
                    case hash("date"): // : Sat, 18 Mar 2023 10 : 06 : 44 GMT
                        _date = string(pcolon);
                        break;
                    case hash("content-type"): // : text / html
                        _content_type = string(pcolon);
                        break;
                    case hash("content-length"):
                        _content_lenght = std::atoi(pcolon);
                        break;
                    case hash("connection"):
                        _connection = string(pcolon);
                        break;
                    case hash("location"): // : https://www.fast-report.com/index.html
                        _location = string(pcolon);
                        break;
                    case hash("x-frame-options"):
                        _x_frame_options = string(pcolon);
                        break;
                    case hash("content-security-policy"):
                        _content_security_policy = string(pcolon);
                        break;
                    case hash("accept-ranges"):
                        _accept_ranges = string(pcolon);
                        break;
                    case hash("cache-control"):
                        _cache_control = string(pcolon);
                        break;
                    case hash("etag"):
                        _etag = string(pcolon);
                        break;
                    default:
                        printf("Unparsed HTTP option in resonse: \033[36m%s\033[0m: %s\n", headline, pcolon);
                        break;
                    }
                    continue;
                }
                // printf("Got header deimiter\n");
                sync = true;
                continue;

            case cr:
                if (*src == '\n')
                    state = option;
                continue;
            }
        }

        return src;
    }
}
