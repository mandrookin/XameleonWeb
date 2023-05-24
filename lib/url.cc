#include "../http.h"
#include <stdint.h>
#include <cstdlib>

namespace xameleon
{
    const char * url_s::get_method()
    {
        switch (method)
        {
        case POST: return "POST";
        case GET:  return "GET";
        case PUT:  return "PUT";
        case PATCH: return "PATCH";
        case DEL: return "DEL";
        }
        return "ERROR";
    }

    const char* url_s::get_http_version()
    {
        switch (version.human)
        {
        case v0_9: return "HTTP/0.9";
        case v1_0: return "HTTP/1.0";
        case v1_1: return "HTTP/1.1";
        case v2_0: return "HTTP/2.0";
        }
        return "HTTP/1.0";
    }

    bool url_s::parse(const char* url)
    {
        const int default_port = 80;
#ifdef BIG_ENDIAN
        const int32_t http_sequence = 0;
        const uint16_t scheme_delimiter = 0;
#else
        const int32_t http_sequence = 0x70747468;
        const uint16_t scheme_delimiter = 0x2f2f;
#endif
        this->domainname = nullptr;
        this->pagename = nullptr;
        this->parameters_count = 0;
        this->anchor = nullptr;

//        int pos = 4;
        int16_t scheme_div = 0;
        int32_t * prefix = (int32_t*)url;
        auto a = *prefix;
        if (a != http_sequence) {
            // Пока забиваем на всё, что не HTTP и не HTTPS
            return false;
        }
        url += sizeof(int32_t);
        this->scheme = HTTP;
        if (*url == 's') {
            this->scheme = HTTPS;
            url++;
        }
        if (*url++ != ':')
            return false;
        scheme_div = *url++ << 8 | *url++;
        if (scheme_div != scheme_delimiter)
            return false;

        // Эта часть разбирает доменное имя и порт 
        this->port = default_port;
        this->domainname = this->path;
        char* p = this->path;;
        bool domain = true;
        do switch (*url) {
        case 0:
            *p++ = 0;
            domain = false;
            continue;
        case ':':
        {
            *p++ = 0;
            url++;
            const char* port_ptr = p;
            while (*url != 0 && *url != '/' && *url != '?' && *url != '#')
                *p++ = *url++;
            *p++ = 0;
            this->port = std::atoi(port_ptr);
            if (this->port < 1 && this->port > 65535)
                return false;
            domain = false;
            continue;
        }
        default:
            *p++ = *url++;
        } while (domain);

#if true
        if (*url)
            get_http_path(url, this);
        else {
            pagename = p;
            *p++ = '/';
            *p++ = 0;
        }

#else
        if (*url == '/') {
            this->domainname = p;
            do 
                *p++ = *url++;
            while (*url != 0 && *url != '?' && *url != '#');
            *p = 0;
        }

        if (*url == '?') {

        }

        switch (*url)
        {
        case 0:
            return true;
        case '#':
            url++;
            anchor = p;
            while (*url)
                *p++ = *url++;
            return true;
        case '?':
                

        default:
            throw "URL parser algorithm errorr";
        }
#endif
        return true;
    }

}