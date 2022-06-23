#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <cstdlib>

// 中国

/*
    Track by
1. Cookie
2. Session Context
3. Etag
4. Browser localStorage
*/

#include "http.h"
#include "session.h"
#include "action.h"

const int max_header_size = 1024;

void http_response_t::clear()
{
    if (_body != nullptr) {
        fprintf(stderr, "Body is not released! Looks like memory leak/ Fix it asap!\n");
    }
    _cookies.clear();
}

void http_response_t::release()
{
    if (_body != nullptr) {
        delete _body;
        _body = nullptr;
    }
    _code = 200;
    _max_age = 0;
    _body_size = 0;
    _header_size = 0;
    etag.clear();  // А что с кэшем тогда?
    _cookies.clear();
}


int http_response_t::add_cookies_to_header(char * buffer, int buffsz)
{
    int size = 0;
    int count = _cookies.size();
    if ( count != 0) {
        for (auto const& cookie : _cookies) {
            char tbuf[80];
            *tbuf = '\0';
            if (cookie.second.lifetime > 0) {
                time_t now = time(nullptr) + cookie.second.lifetime;
                struct tm tm = *gmtime(&now);
                strftime(tbuf, sizeof tbuf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

                size += snprintf(buffer + size, buffsz - size,
                    "Set-Cookie: %s=%s; Expires=%s; Secure; Path=/",
                    cookie.first.c_str(), cookie.second.value.c_str(), tbuf);
            }
            else
            {
                size += snprintf(buffer + size, buffsz - size,
                    "Set-Cookie: %s=%s; Path=/",
                    cookie.first.c_str(), cookie.second.value.c_str() );
            }

            count--;
        }
        size += snprintf(buffer + size, buffsz - size, "\r\n");
    }
    return size;
}

void http_response_t::add_cookie(const char * name, const char *value, unsigned int seconds)
{
    _cookies[name] = { value, seconds };
}

int http_response_t::redirect_to(int code, const char * url, bool keep_alive)
{
    int size = 
    snprintf(_header, sizeof(_header), // Будь осторожен, впоследствии header будет указателем на локальные данные и тогда от sizeof всё рассыпется
        "HTTP/1.1 %u Found\r\n"
        "Connection: %s\r\n"
        "",
        code,
	keep_alive ? "keep-alive" : "close"
    );
    size += add_cookies_to_header(_header + size, sizeof(_header) - size);
    size += snprintf(_header + size, sizeof(_header) - size, "Location: %s\r\n\r\n", url);
    printf("\033[31m%s\033[0m\n", _header);
    return size;
}

int http_response_t::prepare_header(char * header, int code, int body_size)
{
    char tbuf[80];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(tbuf, sizeof tbuf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    int header_size = snprintf(header, 1024,
        "HTTP/1.1 %d OK\r\n"
        "Date: %s\r\n"
        "Server: Pressure/1.0.0 (POSIX)\r\n"
        "Connection: keep-alive\r\n"
        "Content-Type: %s\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %d\r\n",
        code, tbuf, content_type.c_str(), body_size);
    
    if (!etag.empty()) 
    {
        header_size += snprintf( header + header_size, 1024 - header_size,
            "Cache-Control: private, max-age=%u, must-revalidate, proxy-revalidate\r\n"
            "ETag: %s\r\n", 
            _max_age, etag.c_str()
        );
    }
    header_size += add_cookies_to_header(header + header_size, 1024 - header_size);
    header_size += snprintf(header + header_size, 1024 - header_size, "\r\n");
    return header_size;
}

void http_request_t::prepare()
{
    _content_lenght = 0;
    _cookies.clear();
    ranges_count = 0;
    encoding = plain;
}

void http_request_t::parse_range(char* rangestr)
{
    ranges_count = 0;
    if (strncmp(rangestr, "bytes=", 6) == 0) {
        rangestr += 6;
        char* start = nullptr;
        char* delim = nullptr;
        while(*rangestr) {
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
                if(ranges_count < max_range_count)
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

void http_request_t::parse_cache_control(char * cache_control)
{
    this->_cache_control = cache_control;
    printf("Cache control: %s\n", cache_control);
}

void http_request_t::parse_cookies(char * cookie)
{
    char * term;
    do {
        term = strstr(cookie, "; ");
        if (term)
            *term = '\0';
        char * delim = strchr(cookie, '=');
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

void http_request_t::parse_encoding(char* enc)
{
    // gzip, deflate, br
    encoding = plain;
    char* ptr = enc;
    while(true) 
    {
        if (*ptr == ',' || *ptr == '\0') {
            int len = ptr - enc;
            if (len == 4 && enc[0] == 'g' && enc[1] == 'z' && enc[2] == 'i' && enc[3] == 'p') {
                encoding = (encoding_t) (encoding | gzip);
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

char * http_request_t::parse_http_header(char * header)
{
    body = nullptr;
    char * line = header;
    prepare();
    do {
        char * sterm = strstr(line, "\r\n");
        if (!sterm) {
            fprintf(stderr, "Http header format error 1: %s\n", line);
            return nullptr;
        }
        *sterm = '\0';
        if (sterm == line) {
            //printf("HTTP Header parsed\n");
            line += 2;
            break;
        }
        char * delim = strchr(line, ':');
        if (!delim || delim[1] != ' ') {
            fprintf(stderr, "Http header format error 2\n");
            return nullptr;
        }
        *delim = '\0';
        delim += 2;

        switch (hash(line))
        {
        case hash("Host"):
            _host = delim;
            break;
        case hash("Connection"):
            break;
        case hash("Referer"):
            _referer = delim;
            printf("Referer: %s\n", delim);
            break;
        case hash("Cache-Control"):
            parse_cache_control(delim);
            break;
        case hash("Cookie"):
            parse_cookies(delim);
            break;
        case hash("User-Agent"):
            break;
        case hash("Accept"):
            break;
        case hash("Accept-Encoding"):
            parse_encoding(delim);
//            printf("Encoding code: 0x%02x\n", encoding);
            break;
        case hash("Accept-Language"):
            break;
        case hash("If-None-Match"):
            break;
        case hash("Upgrade-Insecure-Requests"):
            break;
        case hash("Range"):
            parse_range(delim);
            break;
        case hash("Sec-Fetch-Site"):
            break;
        case hash("Sec-Fetch-Mode"):
            break;
        case hash("Sec-Fetch-User"):
            break;
        case hash("Sec-Fetch-Dest"):
            break;
        case hash("sec-ch-ua"):
            break;
        case hash("sec-ch-ua-mobile"):
            break;
        case hash("sec-ch-ua-platform"):
            break;
        // Found these values on HTTP PUT request
        case hash("Content-Length"):
            _content_lenght = std::atoi(delim);
            printf("\033[36mContent-lenght: %d\033[0m\n", _content_lenght);
            break;
        case hash("Origin"):
            break;
        case hash("Content-Type"):
            _content_type = delim;
            break;

        default:
            printf("Unparsed HTTP tag: %s value: %s\n", line, delim);
            break;
        }
        line = sterm + 2;
    } while (true);
    body = line;
    return line;
}

void https_session_t::log(url_t &url, char* request_body)
{
    ///            printf("\tPATH:%s\n\tREST:%s\n", url.path, url.rest);

    printf("%s %s\n\033[33m%s\033[0m\n",
        url.method == POST ? "POST" :
        url.method == GET ? "GET" :
        url.method == PUT ? "PUT" :
        url.method == PATCH ? "PATCH" :
        url.method == DEL ? "DEL" : "Error:",
        url.path, request_body);
}


http_response_t * https_session_t::page_not_found(http_method_t method, char * url, const char* referer)
{
    response_holder._code = 404;
    response_holder.content_type = "text/html; charset=utf-8";
    response_holder._body = new char[4096];
    response_holder._body_size = snprintf(response_holder._body, 4096,
        "<html><head><title>Page not found</title></head><body><p>"
        "Method %s<br>Page '%s' not found<br>"
        "</p><p><a href='%s'</p></body>",
        method == POST ? "POST" :
        method == GET ? "GET" :
        method == PUT ? "PUT" :
        method == PATCH ? "PATCH" :
        method == DEL ? "DEL" : "-Unknown-",
        url, referer ? referer : ""
        );
    response_holder._header_size = response_holder.prepare_header(response_holder._header, 404, response_holder._body_size);

    return &response_holder;
}

void prepare_file( https_session_t * session, url_t * url);

void * https_session_t::https_session()
{
    url_t           url;
    int             rcv_sz;
    char            rcv_buff[4096];

    transport->handshake();

    while (1) 
    {
#if CONNECTION_TIMEOUT  // TODO
        fd_set set;
        struct timeval timeout;

        /* Initialize the file descriptor set. */
        FD_ZERO(&set);
        FD_SET(context->client, &set);

        /* Initialize the timeout data structure. */
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        /* select returns 0 if timeout, 1 if input available, -1 if error. */
        int ev = select(context->client + 1, &set, NULL, NULL, &timeout);
        //  if(ev == 0) { printf("Socket timeout\n"); continue; }
        //  if(ev < 0) { printf("Terminate waiting\n"); break; }
        if (ev <= 0) { printf("Closing connection on timeout\n"); break; }
#endif
        const long long timeout_5_minutes = 100000 * 60 * 5;
        const long long timeout_5_seconds = 100000 * 5;
        rcv_sz = transport->recv(rcv_buff, 4096, transport->is_secured() ?  timeout_5_minutes : timeout_5_seconds);
        if (rcv_sz <= 0) break;
        rcv_buff[rcv_sz] = 0;

        //printf("\033[34m%s\033[0m", rcv_buff);
        //fflush(stdout);

        char * header_body = parse_http_request(rcv_buff, &url);

        if (header_body == nullptr) {
            fprintf(stderr, "Got broken HTTP request: %s size: %u\n%s\n", url.path, rcv_sz, rcv_buff);
            break;
        }

        char * request_body = request.parse_http_header(header_body + 2);
        if (!request_body) {
            fprintf(stderr, "HTTP header error\n");
        }
        cookie_found = request._cookies.count("lid") != 0;

        http_response_t * response = nullptr;

        http_action_t * action = find_http_action(url);
        if (!action) {
            response = page_not_found(url.method, url.path, request._referer.c_str());
            if (request._content_lenght != 0) {
                // Рвём соединение если нам пытаются залить какие-то данные, а их некуда лить. 
                // Это лучше, чем попусту тянуть возможно мегабайты данных
                transport->send(response->_header, response->_header_size);
                transport->send(response->_body, response->_body_size);
                break;
            }
        }
        else if (transport->is_secured() == 0) {
            response = &response_holder;
            char    jump_to[1024];
            if (request._referer.size() > 0) {
                printf("TODO: catch reference from '%s'\n", request._referer.c_str());
            }
            printf("Redirect tp host: %s\n", request._host.c_str());
            snprintf(jump_to, sizeof(jump_to) - 1, "https://%s", request._host.c_str());
            response->_header_size = response->redirect_to(302, jump_to, false);
        }
        else {
            //printf("Rights: %d Cookie found: %d\n", action->get_rights(), cookie_found);
            if (action->get_rights() == tracked && !cookie_found /*&& !session_found*/ ) {
                response = &response_holder;
                //url.rest = (char*)"touch.html";
                strcpy(url.path, "touch.html");
                prepare_file(this, &url);
                response->_code = 200;
                response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);
                transport->send(response->_header, response->_header_size);
                transport->send(response->_body, response->_body_size);
                response->release();
                continue;
            }
            response = action->process_req(this, &url);
        }

        log(url, request_body);

        //printf("Response header size = %d\n", response->_header_size);
        transport->send(response->_header, response->_header_size);
        if (response->_body_size) {
            //printf("Response body size = %d\n", response->_body_size);
            transport->send(response->_body, response->_body_size);
        }
        response->release();
    }
    if (rcv_sz == 0) {
        printf("Client closed connection\n");
    }
    else if (rcv_sz < 0) {
        fprintf(stderr, "SSL connection error\n");
    }
    transport->close();
    return nullptr;
}
