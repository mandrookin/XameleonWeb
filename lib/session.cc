#define _CRT_SECURE_NO_WARNINGS 1

#pragma execution_character_set( "utf-8" )

// Miltipart content description
// https://learn.microsoft.com/en-us/previous-versions/office/developer/exchange-server-2010/aa493937(v=exchg.140)

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
  #include <openssl/ssl.h>
  #include <openssl/err.h>
#else
#include <time.h>
//#include <winsock2.h>
#endif
#include <cstdlib>

// 中国

/*
    Track by
1. Cookie
2. Session Context
3. Etag
4. Browser localStorage
*/

#include "../http.h"
#include "../transport.h"
#include "../session.h"
#include "../action.h"

namespace xameleon {

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
//            printf("\n\n%s\n\n", _body);
            delete _body;
            _body = nullptr;
//            printf("---\n");
        }
        _code = 200;
        _max_age = 0;
        _body_size = 0;
        _header_size = 0;
        etag.clear();  // А что с кэшем тогда?
        _cookies.clear();
    }


    int http_response_t::add_cookies_to_header(char* buffer, int buffsz)
    {
        int size = 0;
        int count = (int) _cookies.size();
        if (count != 0) {
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
                        cookie.first.c_str(), cookie.second.value.c_str());
                }

                count--;
            }
            size += snprintf(buffer + size, buffsz - size, "\r\n");
        }
        return size;
    }

    void http_response_t::add_cookie(const char* name, const char* value, unsigned int seconds)
    {
        _cookies[name] = { value, seconds };
    }

    int http_response_t::redirect_to(int code, const char* url, bool keep_alive)
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

    int http_response_t::prepare_header(char* header, int code, int body_size)
    {
        char tbuf[80];
        time_t now = time(0);
        struct tm tm = *gmtime(&now);
        strftime(tbuf, sizeof tbuf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

        const char* format =
            "HTTP/1.1 %d OK\r\n"
            "Date: %s\r\n"
            "Server: Pressure/1.0.0 (POSIX)\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: %s\r\n"
            "Accept-Ranges: bytes\r\n";

        int header_size = snprintf(header, 1024, format,
            code, tbuf, _content_type.c_str(), body_size);
        if (body_size)
            header_size += snprintf(header + header_size, 1024- header_size, 
                "Content-Length: %d\r\n", body_size);

        if (!etag.empty())
        {
            header_size += snprintf(header + header_size, 1024 - header_size,
                "Cache-Control: private, max-age=%u, must-revalidate, proxy-revalidate\r\n"
                "ETag: %s\r\n",
                _max_age, etag.c_str()
            );
        }
        header_size += add_cookies_to_header(header + header_size, 1024 - header_size);
        header_size += snprintf(header + header_size, 1024 - header_size, "\r\n");
        return header_size;
    }


    http_response_t* https_session_t::page_not_found(http_method_t method, char* url, const char* referer)
    {
        response_holder._code = 404;
        response_holder._content_type = "text/html; charset=utf-8";
        response_holder._body = new char[4096];
        response_holder._body_size = snprintf(response_holder._body, 4096,
            "<html><head><title>Page not found</title></head><body><p>"
            "Method %s<br>Page '%s' not found<br>"
            "</p><p><a href='%s'</p></body></html>\r\n",
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

    void https_session_t::log(char* request_body)
    {
        ///            printf("\tPATH:%s\n\tREST:%s\n", url.path, url.rest);
        printf("%s %s\n\033[33m%s\033[0m\n",
            request.url.method == POST ? "POST" :
            request.url.method == GET ? "GET" :
            request.url.method == PUT ? "PUT" :
            request.url.method == PATCH ? "PATCH" :
            request.url.method == DEL ? "DEL" : "Error:",
            request.url.path, request_body);
    }

    int https_session_t::tracky_response(url_t* url)
    {
        strcpy(url->path, "touch.html");
        response_send_file(this);
        return 0;
    }

    void* https_session_t::https_session()
    {
        reader->get_transport()->handshake();

        while (true)
        {
            const char * src = request.parse_request(reader);
            counters.total_request++;
            if (!src) {
                counters.bad_request++;
                break;
            }
            else if (src == reader->zero_socket) {
                fprintf(stderr, "Close on remote closed connection: ");
                break;
            }
            else if (src == reader->error_socket) {
                fprintf(stderr, "Socker error: ");
                break;
            }

            cookie_found = request._cookies.count("lid") != 0;

            if (request.x_forward_for != 0)
                counters.x_forward_counters++;
            if (request.x_real_ip != 0)
                counters.x_realip_counters++;

            http_response_t* response = nullptr;

            http_action_t* action = find_http_action(request.url);
            if (!action) {
                counters.not_found++;
                printf("Not found counters = %d\n", counters.not_found);
                response = page_not_found(request.url.method, request.url.path, request._referer.c_str());
                if (request._content_lenght != 0) {
                    // Рвём соединение если нам пытаются залить какие-то данные, а их некуда лить. 
                    // Это лучше, чем попусту тянуть возможно мегабайты данных
                    reader->get_transport()->send(response->_header, response->_header_size);
                    reader->get_transport()->send(response->_body, response->_body_size);
                    break;
                }
            }
#ifdef HTTP_REDIRECT
            else if (reader->get_transport()->is_secured() == 0) {
                counters.http2https++;
                response = &response_holder;
                char    jump_to[1024];
                if (request._referer.size() > 0) {
                    printf("TODO: catch reference from '%s'\n", request._referer.c_str());
                }
                printf("Redirect to host: %s\n", request._host.c_str());
                snprintf(jump_to, sizeof(jump_to) - 1, "https://%s", request._host.c_str());
                response->_header_size = response->redirect_to(302, jump_to, false);
            }
#endif
            else {
                //printf("Rights: %d Cookie found: %d\n", action->get_rights(), cookie_found);
                if (action->get_rights() == tracked && !cookie_found /*&& !session_found*/) {
                    this->tracky_response(&request.url);
                    continue;
                }
                response = action->process_req(this);
            }

            // log(url, request_body);

            if (response) 
            {
                // printf("Response header size = %d\n", response->_header_size);
                if (response->_header_size) {
                    reader->get_transport()->send(response->_header, response->_header_size);
                    // printf("Response body size = %d\n", response->_body_size);
#if DEBUG_RESPONSE
                    FILE* fd = fopen("raw/http.raw", "wb");
                    if (fd) {
                        fwrite(response->_body, response->_body_size, 1, fd);
                        fclose(fd);
                    }
#endif
                    if (response->_body_size) {
                        reader->get_transport()->send(response->_body, response->_body_size);
                    }
                }
                response->release();
            }
        }
        reader->get_transport()->close();
        return nullptr;
    }
}

#ifdef  _WIN32
extern xameleon::transport_t* create_file_descriptor_transport(FILE* fp);

DWORD http_thread(void* arg)
{
    xameleon::https_session_t* client = (xameleon::https_session_t*)arg;
//    LONG prev_value;

    DWORD id = GetCurrentThreadId();
    client->https_session();

    //for (int i = 0; i < 5; i++)
    //    ev = new event_t;

    //    ev->event_name = "Thread #" + to_string(id);
    //    ev->temperature = get_float_random_number();

    //    // Следующие 4 строки - передача сообщения
    //    EnterCriticalSection(&my_critical_sction);
    //    pogoda.push_back(ev);
    //    LeaveCriticalSection(&my_critical_sction);
    //    ReleaseSemaphore(hSemaphore, 1, &prev_value);
    //}
    delete client;
    client = nullptr;

    return 0;
}


int test_full_http(const char * name_of_raw_file)
{
    xameleon::http_action_t* static_page = new xameleon::static_page_action(xameleon::guest);
    xameleon::http_action_t* proxy = new xameleon::proxy_action;
    add_action_route("/", xameleon::GET, static_page);
    add_action_route("/favicon.ico", xameleon::GET, new xameleon::get_favicon_action);
    add_action_route("/proxy", xameleon::GET, proxy);
    add_action_route("/proxy/", xameleon::GET, proxy);
    add_action_route("/proxy", xameleon::PUT, proxy);
    add_action_route("/proxy/", xameleon::PUT, proxy);
    add_action_route("/proxy", xameleon::POST, proxy);
    add_action_route("/proxy/", xameleon::POST, proxy);
    add_action_route("/proxy", xameleon::DEL, proxy);
    add_action_route("/proxy/", xameleon::DEL, proxy);

#if FILE_TEST
    FILE* fp = fopen(name_of_raw_file, "rb");
    if (fp == NULL)
    {
        perror("Unable open multipart file");
        return -1;
    }
    xameleon::transport_t* transport = create_file_descriptor_transport(fp);
    xameleon::https_session_t   test_session(transport, "tmp/");
    test_session.https_session();
    fclose(fp);
#else
    xameleon::transport_t* transport = xameleon::create_http_transport();
    xameleon::transport_i* client_transport = nullptr;

    transport->bind_and_listen(80);

    while (true)
    {
        client_transport = transport->accept();
        if (!client_transport) {
            if (true)
                perror("Unable accept incoming connnection");
            else
                puts("\033[36m\nServer stopped by TERM signal\033[0m\n");
            break;
        }
        xameleon::https_session_t* client = new xameleon::https_session_t(client_transport, "pages");

        DWORD  thread_id = 0;
        HANDLE thrd = CreateThread(nullptr, 0, http_thread, (void*)client, 0, &thread_id);

        if (thrd == nullptr) {
            perror("Unable create client thread");
            delete client;
        }
    }
#endif

    if (transport) {
        transport->close();
        delete transport;
        transport = nullptr;
    }

    return 0;
}

extern "C" int __stdcall SetConsoleOutputCP(
    _In_ unsigned int  wCodePageID
);

extern "C" int main(int argc, char* argv[])
{
    int result;
    // Установка кодировки консоли в UTF-8
    SetConsoleOutputCP(65001);

    int test_full_http(const char* name_of_raw_file);
    result = test_full_http("raw/rcv_sock-1.raw");
    //    result = test_full_http("raw/p.mhtml");
    return result;
}
#endif
