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
#include <vector>

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
#include "../lib.h"
#include "../session_mgr.h"
#include <locale>

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
            printf("\n\n%s\n\n", _body);
            delete _body;
            _body = nullptr;
            printf("---\n");
        }
        _code = 200;
        _max_age = 0;
        _body_size = 0;
        _header_size = 0;
        _etag.clear();  // А что с кэшем тогда?
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
                "HTTP/1.1 %d Found\r\n"
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

    const char* const format =
        "HTTP/1.1 %d OK\r\n"
        "Date: %s\r\n"
        "Server: Xameleon/1.0.0\r\n"
        "Content-Type: %s\r\n"
        "Accept-Ranges: bytes\r\n";

////    char debug_buff[1024];

    int http_response_t::prepare_header(char* header, int code, int body_size)
    {
        char tbuf[256];
        time_t now = time(0);
        _locale_t locale = _get_current_locale();
        struct tm * tm = gmtime(&now);

////        _strftime_l(debug_buff, sizeof(debug_buff), "%a, %d %b %Y %H:%M:%S %Z", tm, locale);
        strftime(tbuf, sizeof(tbuf), "%a, %d %b %Y %H:%M:%S %Z", tm);

        int header_size = snprintf(header, 1024 /*bullshit?*/, format,
            code, tbuf, _content_type.c_str());
        if (body_size) {
            header_size += snprintf(header + header_size, 1024 - header_size,
                "Connection: keep-alive\r\nContent-Length: %d\r\n", body_size);
        }

        if (!_etag.empty())
        {
            header_size += snprintf(header + header_size, 1024 - header_size,
                "Cache-Control: private, max-age=%u, must-revalidate, proxy-revalidate\r\n"
                "ETag: %s\r\n",
                _max_age, _etag.c_str()
            );
        }
        header_size += add_cookies_to_header(header + header_size, 1024 - header_size);
        header_size += snprintf(header + header_size, 1024 - header_size, "\r\n");
        return header_size;
    }

    const char* cszLocalStyletyle =
        "        * { "
        "margin: 0; "
        "padding : 0; "
        "border : 0; "
        "outline : 0; "
        "font-size: 100%%; "
        "vertical-align: baseline;"
"       background: transparent;"
        " }";

    const char * cszPageNotFound =
        "<!DOCTYPE html>"
        "<html><head><title>Page not found</title></head>"
        "<style>%s</style>"
        "<body>"
            "<br/>"
            "<div style=\"width:100%%\">"
                "<div style=\"float:left; width:25%%\"><br/>"
                    "<br/><img src=\"/img/cryface.svg\" alt=\"Web\" class=\"avatar\">"
                "</div>"
                "<div style=\"float:right; width:70%%; padding-left:25px\"><br/>"
                    "<h2><br/>%s %s not found<br></h2><br/>"
                    "<br/>%s"
                "</div>"
            "</div>"
        "</body></html>";

    void https_session_t::page_not_found(http_method_t method, const char* page, const char* referer)
    {
        std::string                     stacked_content_type = response_holder._content_type;;
        char ref[2048];
        char body[4096];

        referer = referer ? referer : "";
        response_holder._code = 404;
        response_holder._content_type = "text/html; charset=utf-8";
        response_holder._body = body;
        ref[0] = 0;
        if (referer && referer[0] != 0)
            snprintf(ref, sizeof(ref), "Referenced from %s", referer);
        response_holder._body_size = snprintf(
            (char*) response_holder._body, sizeof(body), cszPageNotFound,
            cszLocalStyletyle, http_method(method), page, ref);
        response_holder._header_size = response_holder.prepare_header(response_holder._header, 200 /*404*/, response_holder._body_size);
        get_transport()->send(response_holder._header, response_holder._header_size);
        get_transport()->send(response_holder._body, response_holder._body_size);
        response_holder._content_type = stacked_content_type;
    }

    void https_session_t::site_not_found(const char * reqested_host)
    {
        // Здесь есть интересная воможность включить PROXY автоматически
        std::string host_message(reqested_host);
        host_message += " - host ";

        page_not_found( request.url.method, host_message.c_str(), request._referer.c_str() );
        // А вот тут неплохо бы было вывести приветствие, с показом поддерживаемых доменов
    }

    void https_session_t::log(char* request_body)
    {
        ///            printf("\tPATH:%s\n\tREST:%s\n", url.path, url.rest);
        printf("%s %s\n\033[33m%s\033[0m\n", 
            http_method(request.url.method),
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
                if (request.host == nullptr)
                {
                    if (request.url.version.major == 1 && request.url.version.minor == 0) {
                        // https://stackoverflow.com/questions/56331503/when-would-you-get-a-web-request-without-a-host-name
                        printf("Default page is not implemented yet\n");
                    }
                    this->site_not_found(request._cache_control.c_str());
                }
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

            if (request._x_forward_for != 0)
                counters.x_forward_counters++;
            if (request._x_real_ip != 0)
                counters.x_realip_counters++;

            get_active_sessions()->touch_page(request.url);

            http_response_t* response = nullptr;

            http_action_t* action = nullptr;

            if (request.host->mode != proxy)
            {
                action = find_http_action(request);
                if (!action) {
                    counters.not_found++;
                    printf("Not found counters = %d\n", counters.not_found);
                    page_not_found(request.url.method, request.url.path, request._referer.c_str());
#if TODO
                    if (request._content_lenght != 0) {
                        // Рвём соединение если нам пытаются залить какие-то данные, а их некуда лить. 
                        // Это лучше, чем попусту тянуть возможно мегабайты данных
                        reader->get_transport()->send(response->_header, response->_header_size);
                        reader->get_transport()->send(response->_body, response->_body_size);
                        break;
                    }
#endif
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
                    action->process_req(this);
                }
            }
            else
            {
                //////long long rcv_timeout = (long long)1000000 * this->request.host->http_session_maxtime;
                //////this->get_multiart_reader()->set_receive_timer(rcv_timeout);
                this->reverse_proxy();
            }

            // log(url, request_body);

#if false
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
            }
#endif
            if(response)
                response->release();
        }
        reader->get_transport()->close();
        return nullptr;
    }
}

#ifdef  _WIN32
extern xameleon::transport_i* create_file_descriptor_transport(FILE* fp);

DWORD http_thread(void* arg)
{
    xameleon::https_session_t* client = (xameleon::https_session_t*)arg;
    char client_name[128];

    DWORD id = GetCurrentThreadId();

//---
    client->get_transport()->describe(client_name, sizeof(client_name));
    printf("+ HTTP session: %s - ", client_name);
    xameleon::get_active_sessions()->add_session(client);
//    pthread_detach(pthread_self());
    client->https_session();
    xameleon::get_active_sessions()->remove_session(client);
//    delete client;
    printf("- HTTP session: %s\n", client_name);
  //  pthread_exit(NULL);
    //---


    delete client;
    client = nullptr;

    return 0;
}


int test_full_http(const char * name_of_raw_file)
{
    xameleon::http_action_t* static_page = new xameleon::static_page_action(xameleon::guest);
    xameleon::http_action_t* proxy = new xameleon::proxy_action;
    xameleon::http_action_t* cgi_bin = new xameleon::cgi_action(xameleon::guest);
    add_action_route("/", xameleon::GET, static_page);
    add_action_route("/", xameleon::POST, new xameleon::post_form_action(xameleon::guest));
    add_action_route("/favicon.ico", xameleon::GET, new xameleon::get_favicon_action);
    add_action_route("/proxy", xameleon::GET, proxy);
    add_action_route("/proxy/", xameleon::GET, proxy);
    add_action_route("/proxy", xameleon::PUT, proxy);
    add_action_route("/proxy/", xameleon::PUT, proxy);
    add_action_route("/proxy", xameleon::POST, proxy);
    add_action_route("/proxy/", xameleon::POST, proxy);
    add_action_route("/proxy", xameleon::DEL, proxy);
    add_action_route("/proxy/", xameleon::DEL, proxy);
    add_action_route("/upload", xameleon::GET, new xameleon::get_directory_action(xameleon::guest));
    add_action_route("/admin/", xameleon::GET, new xameleon::admin_action);
    add_action_route("/cgi/", xameleon::GET, cgi_bin);
    add_action_route("/cgi/", xameleon::POST, cgi_bin);
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
    xameleon::transport_i* transport = xameleon::get_http_transport("server");
    xameleon::transport_i* client_transport = nullptr;

    transport->bind_and_listen(80);

//    int accepted_connections_count = 0;
    const int MAX_CONNECTIONS = 4;
    
    std::list<HANDLE>     thread_list;

    while (true)
    {
        if (thread_list.size() == MAX_CONNECTIONS) {
            DWORD return_value;
            std::vector<HANDLE> v { std::begin(thread_list), std::end(thread_list) };
            return_value = WaitForMultipleObjects( (DWORD) v.size(), v.data(), FALSE, INFINITE);
            if (return_value >= WAIT_OBJECT_0 && return_value < WAIT_OBJECT_0 + v.size() ) {
                thread_list.remove(v[return_value - WAIT_OBJECT_0]);
            }
        }
        client_transport = transport->accept();
        if (!client_transport) {
            if (true)
                perror("Unable accept incoming connnection");
            else
                puts("\033[36m\nServer stopped by TERM signal\033[0m\n");
            break;
        }

//        ++accepted_connections_count;

        xameleon::https_session_t* client = new xameleon::https_session_t(client_transport);

        DWORD  thread_id = 0;
        HANDLE thrd = CreateThread(nullptr, 0, http_thread, (void*)client, 0, &thread_id);
        

        if (thrd == nullptr) {
            perror("Unable create client thread");
            delete client;
        }

        thread_list.push_back(thrd);
    }
#endif

    if (transport) {
        transport->close();
        delete transport;
        transport = nullptr;
    }

    return 0;
}

//extern "C" int __stdcall SetConsoleOutputCP(
//    _In_ unsigned int  wCodePageID
//);

BOOL WINAPI CTRLCHandlerRoutine( _In_ DWORD dwCtrlType )
{
    printf("TODO: Catch CTRL+C under windows\n");
    return TRUE; // We catch event. No more habdlers will be called
}

extern "C" int main(int argc, char* argv[])
{
    int result;
    // Установка кодировки консоли в UTF-8
    SetConsoleOutputCP(65001);
    SetConsoleCtrlHandler(CTRLCHandlerRoutine, TRUE);

    int test_full_http(const char* name_of_raw_file);
    result = test_full_http("raw/rcv_sock-1.raw");
    //    result = test_full_http("raw/p.mhtml");
    return result;
}
#endif
