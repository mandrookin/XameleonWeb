#pragma once
#define _CRT_SECURE_NO_WARNINGS 1
#include <string.h>
#include <ctype.h>

#include "../http.h"
#include "../session.h"

namespace xameleon
{
    /*
    https://datatracker.ietf.org/doc/html/rfc3986
    */

    //
    // До боли оптимизированная функция, но ОЧЕНЬ плохой стиль программирований
    // Быстрее сделать почти не возможно, во всяком случае без оптимзации под кокретную архитектуру
    // Возвращает указатель на поле путь HTTP заголовка
    //

    char* get_http_method(char* hdr, url_t* uri)
    {
        const char G = 'G', E = 'E', T = 'T', P = 'P', U = 'U', O = 'O', S = 'S', SP = ' ';
        char predicate;
        char* next = nullptr;
        uri->method = ERR;
        if (hdr[0] == G) {
            predicate = ((hdr[1] ^ E) | (hdr[2] ^ T) | (hdr[3] ^ SP));
            if (predicate == 0) {
                uri->method = GET;
                next = hdr + 4;
            }
        }
        else if (hdr[0] == P) {
            predicate = ((hdr[1] ^ U) | (hdr[2] ^ T) | (hdr[3] ^ SP));
            if (predicate == 0) {
                uri->method = PUT;
                next = hdr + 4;
            }
            else {
                predicate = ((hdr[1] ^ O) | (hdr[2] ^ S) | (hdr[3] ^ T) | (hdr[4] ^ SP));
                if (predicate == 0) {
                    uri->method = POST;
                    next = hdr + 5;
                }
                else if (strncmp(hdr, "PATCH ", 6) == 0) {
                    uri->method = PATCH;
                    next = hdr + 6;
                }
            }
        }
        else if (strncmp(hdr, "DELETE ", 7) == 0) {
            uri->method = DEL;
            next = hdr + 7;
        }
        return next;
    }

    inline char get_hex_digit(char ch)
    {
        ch = toupper(ch);
        return ch >= 'A' ? 0xa + ch - 'A' : ch - '0';
    }

    //
    // Функция разбора URL на составные части и занесение их в структуру.
    // Возвращает указатель на HTTP заголовок или NULL, если произошла ошибка разбора
    //
    char* get_http_path(char* ptr, url_t* uri)
    {
        const char* allowed_in_path = "/-._~";
        uri->query_count = 0;
        for (int i = 0; i < 32; i++) {
            uri->query[i].key = nullptr;
            uri->query[i].val = nullptr;
        }
        for (int i = 0; i < max_path_len - 1; i++) {
            if (*ptr == ' ' || *ptr == '\0') {
                uri->path_len = i;
                uri->path[i] = '\0';
                return ++ptr;
            }
            if (*ptr == '?') {
                uri->path[i] = '\0';
                ++ptr;
                uri->query[uri->query_count].key = uri->path + i + 1;
                uri->query_count++;
                uri->path_len = i;
                bool key_found = true;
                for (i++; i < max_path_len; i++) {
                    if (*ptr == ' ') {
                        uri->path[i] = '\0';
                        return ++ptr;
                    }
                    if (*ptr == '=' && key_found) {
                        uri->path[i] = '\0';
                        uri->query[uri->query_count - 1].val = uri->path + i + 1;
                        ptr++;
                        key_found = false;
                        continue;
                    }
                    if (*ptr == '&') {
                        uri->path[i] = '\0';
                        uri->query[uri->query_count].key = uri->path + i + 1;
                        uri->query_count++;
                        ptr++;
                        key_found = true;
                        continue;
                    }
                    if (*ptr != '%')
                        uri->path[i] = *ptr++;
                    else {
                        ptr++;
                        uri->path[i] = (get_hex_digit(*ptr) << 4) | get_hex_digit(*(ptr + 1));
                        ptr += 2;
                    }
                }
            }
            if (*ptr != '%') {
                if (!isalnum(*ptr) && !strchr(allowed_in_path, *ptr)) {
                    break;
                }
                uri->path[i] = *ptr++;
            }
            else {
                ptr++;
                uri->path[i] = (get_hex_digit(*ptr) << 4) | get_hex_digit(*(ptr + 1));
                ptr += 2;
            }
        }
        return nullptr;
    }


    char* get_http_version(char* p, url_t* uri)
    {
        const char H = 'H', T = 'T', P = 'P', SL = '/', DOT = '.';
        char predicate = (p[0] ^ H) | (p[1] ^ T) | (p[2] ^ T) | (p[3] ^ P) | (p[4] ^ SL);
        predicate |= p[6] ^ DOT;

        if (predicate)
            return nullptr;

        uri->version.major = p[5] - '0';
        uri->version.minor = p[7] - '0';
        return p + 8;
    }

#include <stdio.h>

    char* parse_http_request(char* hdr, url_t* uri)
    {
        char* next;
        next = get_http_method(hdr, uri);
        if (next) {
            next = get_http_path(next, uri);
            if (next)
                next = get_http_version(next, uri);
            else fprintf(stderr, "HTTP header path error");
        }
        else fprintf(stderr, "HTTP header method error: %s\n", hdr);
        return next;
    }
}


namespace xameleon
{

    void http_request_t::prepare()
    {
        _content_lenght = 0;
        _cookies.clear();
        ranges_count = 0;
        encoding = plain;
        port = 0; // Использовать порт по умолчанию, если не задан
        x_forward_for = 0;
        x_real_ip = 0;
    }

    void http_request_t::parse_range(char* rangestr)
    {
        ranges_count = 0;
        if (strncmp(rangestr, "bytes=", 6) == 0) {
            rangestr += 6;
            char* start = nullptr;
            char* delim = nullptr;
            while (*rangestr) {
                ranges[ranges_count].start = ranges[ranges_count].stop = 0;
                while (*rangestr == ' ') rangestr++;
                if (!*rangestr) break;
                if (isdigit(*rangestr)) {
                    start = rangestr++;
                    delim = nullptr;
                    while (isdigit(*rangestr)) rangestr++;
                    if (*rangestr == '-') {
                        delim = rangestr++;
                        *delim++ = '\0';
                        while (isdigit(*rangestr)) rangestr++;
                    }
                }
                else if (*rangestr == '-') {
                    delim = rangestr++;
                    *delim++ = '\0';
                    while (isdigit(*rangestr)) rangestr++;
                }

                if (start || delim) {
                    if (start) ranges[ranges_count].start = atoi(start);
                    if (delim) ranges[ranges_count].stop = atoi(delim);
                    printf("Detected Range: '%d' - '%d'\n", ranges[ranges_count].start, ranges[ranges_count].stop);
                    if (ranges_count < max_range_count)
                        ranges_count++;
                }
                else {
                    fprintf(stderr, "http_request_t::parse_range: Range not parsed\n");
                }
                if (*rangestr != ',')
                    break;
                rangestr++;
            }
        }
        else
            fprintf(stderr, "Unknown range unit: %s\n", rangestr);
    }

    void http_request_t::parse_cache_control(char* cache_control)
    {
        this->_cache_control = cache_control;
        printf("Cache control: %s\n", cache_control);
    }

    void http_request_t::parse_cookies(char* cookie)
    {
        char* term;
        do {
            term = strstr(cookie, "; ");
            if (term)
                *term = '\0';
            char* delim = strchr(cookie, '=');
            if (!delim) {
                fprintf(stderr, "Cookie format error: %s\n", cookie);
            }
            else {
                *delim++ = '\0';
                //printf("\t%s: %s\n", cookie, delim);
                _cookies[cookie] = delim;
            }
            if (term)
                cookie = term + 2;
        } while (term);
    }

    char* http_request_t::parse_content_type(char* value)
    {
        const char prefix[] = "boundary=";
        const char* type = nullptr;
        const char* subtype = nullptr;

        for (char* p = value; *p; p++)
            switch (context_state)
            {
            case undefined:
                if (*p == '/') {
                    context_state = first_split;
                    *p = 0;
                    type = value;
                    value = p + 1;

                    major_type_t typehash = (major_type_t)hash(type);
                    switch (typehash)
                    {
                    case text:
                    case image:
                    case multipart:
                        this->major_type = typehash;
                        break;
                    default:
                        printf("\033[36mUnknow %p Content-type: %s\033[0m\n", typehash, type);
                    }

                }
                break;
            case first_split:
                if (*p == ';' || *p == '\r')
                {
                    context_state = *p == '\r' ? lf : second_arg;
                    *p = 0;
                    subtype = value;
                    value = p + 1;

                    minor_type_t typehash = (minor_type_t)hash(subtype);
                    switch (typehash)
                    {
                        // multipart
                    case formdata:
                    case related:
                    case mixed:
                        // text
                    case html:
                    case css:
                        // image
                    case gif:
                    case png:
                    case jpg:
                        this->minor_type = typehash;
                        break;

                    default:
                        printf("\033[36mUnknow %p content sub type: %s\033[0m\n", typehash, type);
                        break;
                    }
                }
                break;

            case second_arg:
                if (*p == '\r')
                    context_state = lf;
                break;

            case lf:
                context_state = second_arg;
                break;

            default:
                throw "wrong state in http_request_t::parse_content_type";
            }

//        context_state = undefined;

        char* ptr = strstr(value, prefix);
        if (ptr) {
            ptr += sizeof(prefix) - 1;

            if (*ptr == '"') {
                ptr++;
                for (char* p = ptr; *p; p++)
                    if (*p == '"') {
                        *p = 0;
                        break;
                    }
            }
        }
        return ptr;
    }

    void http_request_t::parse_encoding(char* enc)
    {
        // gzip, deflate, br
        encoding = plain;
        char* ptr = enc;
        while (true)
        {
            if (*ptr == ',' || *ptr == '\0') {
                int len = ptr - enc;
                if (len == 4 && enc[0] == 'g' && enc[1] == 'z' && enc[2] == 'i' && enc[3] == 'p') {
                    encoding = (encoding_t)(encoding | gzip);
                    enc = ++ptr;
                }
                else if (len == 2 && enc[0] == 'b' && enc[1] == 'r') {
                    encoding = (encoding_t)(encoding | br);
                    enc = ++ptr;
                }
                else if (len == 7 && enc[0] == 'd' && enc[1] == 'e' && enc[2] == 'f' && enc[3] == 'l') {
                    encoding = (encoding_t)(encoding | deflate);
                    enc = ++ptr;
                }
            }
            if (*ptr == '\0')
                break;
            if (*ptr == ' ')
                enc++;
            ptr++;
        }
    }

    uint32_t parseIPV4string(char* ipAddress) {
#ifdef _WIN32 // CHECK_ENDIAN_LATER
        char ipbytes[4];
        sscanf(ipAddress, "%hhu.%hhu.%hhu.%hhu", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
        //printf("%u.%u.%u.%u\n", ipbytes[3], ipbytes[2], ipbytes[1], ipbytes[0]);
        return ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
#else
        return inet_addr(ipAddress);
#endif
    }

    void http_request_t::parse_forward_tag(char* ip_string)
    {
        x_forward_for = parseIPV4string(ip_string);
        printf("x-forwarded-for: %s is %08x\n", ip_string, x_forward_for);

    }

    const char* http_request_t::parse_http_header(multipart_stream_reader_t* reader)
    {
        typedef enum { enter, name, delimiter, value, finish, body } state_t;
        char* line;
        char* val = nullptr;
        bool sync = false;
        int rest;
        state_t state = name;
        const char* src_ptr;
        char* prev_dst_ptr = nullptr;
        char  header[4096];

        prepare();

        do {
            char* dst_ptr = header;
            line = header;
            char ch;

            do {
                src_ptr = reader->get_pchar(sync, rest);
                if (sync)
                    break;
                ch = *src_ptr;
                switch (state)
                {
                case enter:
                    if (ch == ' ' || ch == '\t')
                    {
                        state = delimiter;;
                        dst_ptr = prev_dst_ptr;
                        continue;
                    }
                    if (ch == '\r')
                    {
                        state = body;
                        continue;
                    }
                    // not                     break;
                case name:
                    if (ch == ':')
                    {
                        state = delimiter;
                        ch = 0;
                        break;
                    }
                    ch |= 0b00100000;
                    break;
                case delimiter:
                    if (ch == ' ' || ch == '\t')
                        continue;
                    state = value;
                    val = dst_ptr;
                    break;
                case value:
                    if (ch == '\r') {
                        ch = 0;
                        state = finish;
                    }
                    break;
                case finish:
                    if (ch != '\n')
                        throw "Line end error";
                    state = enter;
                    prev_dst_ptr = dst_ptr;
                case body:
                    if (ch != '\n')
                        fprintf(stderr, "HTTP header terminator error\n");
                    *dst_ptr = 0;
                    continue;
                }
                *dst_ptr++ = ch;
            } while (ch != '\n');

            *dst_ptr = 0;
            int linelen = dst_ptr - header;

            if (sync) {
                if (src_ptr == reader->bound) {
                    //                    fprintf(stderr, "TODO: got delimiter on header parsing\n");
                    sync = true;
                    return src_ptr;
                }
                fprintf(stderr, "syncrhonization error on http header\n");
                return 0;
            }

            if (linelen == 0)
                return src_ptr;

            char* pcolon;
            switch (hash(line))
            {
            case hash("host"):
                pcolon = strchr(val, ':');
                if (pcolon) {
                    *pcolon++ = '\0';
                    port = atoi(pcolon);
                }
                if (val != nullptr)
                    _host = val; // delim;
                else
                    fprintf(stderr, "Host option empty\n");
                break;
            case hash("connection"):
                break;
            case hash("x-forwarded-for"):
                x_forward_for = parseIPV4string(val); // delim);
                //                parse_forward_tag(delim);
                break;
            case hash("x-real-ip"):
                x_real_ip = parseIPV4string(val); // delim);
                //                printf("x-real-ip: %s\n", delim);
                break;
            case hash("referer"):
                _referer = val; // delim;
                printf("Referer: %s\n", val); // delim);
                break;
            case hash("cache-control"):
                parse_cache_control(val);  // delim);
                break;
            case hash("cookie"):
                parse_cookies(val); // delim);
                break;
            case hash("user-agent"):
                break;
            case hash("accept"):
                break;
            case hash("accept-encoding"):
                parse_encoding(val); // delim);
                //            printf("Encoding code: 0x%02x\n", encoding);
                break;
            case hash("accept-language"):
                break;
            case hash("if-none-match"):
                break;
            case hash("upgrade-insecure-requests"):
                break;
            case hash("range"):
                parse_range(val);// delim);
                break;
            case hash("sec-fetch-site"):
                break;
            case hash("sec-fetch-mode"):
                break;
            case hash("sec-fetch-user"):
                break;
            case hash("sec-fetch-dest"):
                break;
            case hash("sec-ch-ua"):
                break;
            case hash("sec-ch-ua-mobile"):
                break;
            case hash("sec-ch-ua-platform"):
                break;
                // Found these values on HTTP PUT request
            case hash("content-length"):
                _content_lenght = std::atoi(val);
                printf("\033[36mContent-lenght: %d\033[0m\n", _content_lenght);
                break;
            case hash("origin"):
                break;
            case hash("content-type"):
                _content_type = val; // delim;
                line = parse_content_type(val); // delim);
                if (line) {
                    reader->set_bound(line);
                    this->form_data = new form_data_t(line);
                }
                line = header;
                break;

            default:
                printf("Unparsed HTTP tag: %s value: %s\n", line, val); // delim);
                printf("Compare hashes: %08x value: %08x\n", hash(line), hash("content-Length"));
                break;
            }
            //            line = sterm + 2;
        } while (true);
        return src_ptr;
    }

    // Полжу пока сюда, но этому здесь не место
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
        return "GET";
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

    bool http_request_t::proxy_rewrite_header(char* header_buffer, int* size)
    {
        bool status = false;
        int freesize = *size;
        int wr_sz;
        wr_sz = snprintf(header_buffer, freesize, 
            "%s / %s\r\n"
            "Host: %s:%d\r\n"
            "User-Agent: %s\r\n"
            "Accept: %s\r\n"
            "Accept-Language: %s\r\n"
            "\r\n"
            ,url.get_method(), url.get_http_version(),
            _host.c_str(), port,
            "Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0",
            "text/plain, text/html, img/svg",
            "en-US,en;q=0.9"
        );
        *size = wr_sz;
        status = true;
        return status;
    }

    const char* http_request_t::parse_request(multipart_stream_reader_t* reader)
    {
        bool sync = false;
        int rest;
        const char* src = nullptr;

        do
        {
            char  headline[4096];
            char* ptr = headline;
            do {
                src = reader->get_pchar(sync, rest);
                if (sync)
                    break;
                *ptr++ = *src;
            } while (*src != '\n');

            if (sync)
            {
                if (src == reader->zero_socket)
                {
                    //                    rcv_sz = 0;
                    break;
                }
                else if (src == reader->error_socket)
                {
                    //                    rcv_sz = -1;
                    break;
                }
                else if (src == reader->bound)
                {
                    printf("Got multipart bound\n");
                }

            }
            int linelen = ptr - headline;
            //           rcv_sz = linelen;

            char* header_body = parse_http_request(headline, &url);

            if (header_body == nullptr) {
                fprintf(stderr, "Got broken HTTP request: %s size: %u\n%s\n", url.path, linelen, headline);
                src = nullptr;
                break;
            }

            const char* request_body = parse_http_header(reader);
            if (!request_body) {
                fprintf(stderr, "HTTP header error\n");
                src = nullptr;
            }

        } while (false);
        return src;
    }

}
