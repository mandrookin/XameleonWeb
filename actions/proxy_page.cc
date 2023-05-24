#include "../session.h"
#include "../action.h"
#include "../lib.h"
#include "../multipart_form.h"
#include "../transport.h"


using namespace xameleon;

int proxy_action::load_form(https_session_t* session)
{
    int status = -1;
    http_request_t* request = &session->request;
    form_data_t* data = request->form_data;
    multipart_stream_reader_t* reader = session->get_multiart_reader();

    if (data != nullptr) {

        bool sync = false;
        status = parse_form_data(reader, data, sync);
        if (status < 0) {
            char    remote_transport[64];
            session->get_transport()->describe(remote_transport, sizeof(remote_transport));
            fprintf(stderr, "Unable parse form data from '%s'\n", remote_transport);
        }
        show(data);

        reader->set_bound(nullptr);
        bool signal = false;
        int rest;
        std::string tail;
        do {
            const char* ch = reader->get_pchar(signal, rest);
            if (signal || rest <= 0)
                break;
            tail += *ch;
        } while (true);
    }
    else
    {
        fprintf(stderr, "No bound defined\n");
    }
    return status;
}

#pragma execution_character_set( "utf-8" )

static const char  szProxyScreen[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<meta charset=\"utf-8\">"
    "<title>Прокси режим</title>"
"</head>\r\n"
"<body>"
    "<form id = \"seturl\" enctype=\"multipart/form-data\" method=\"post\" action=\"#\">"
    "Введите URL для перехода<br>"
        "<input type = \"text\" name=\"url\" style=\"border: 2px solid gray; border-radius: 2px\" width=\"120px\" value=\"\" />"
        "<input type = \"submit\" class=\"button\" value=\" Go \"/>"
    "</form>"
    "<br/><br/><a href=\"/\">Вернуться назад </a><br/>"
"</body>"
"</html>\r\n";


void proxy_action::proxy_loop(https_session_t* session)
{
    const char* src = nullptr;
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
    multipart_stream_reader_t   reader(proxy_transport);

    char rewite_header_buffer[2048];
    int hdr_size = sizeof(rewite_header_buffer);

    do {
        if (request->proxy_rewrite_header(rewite_header_buffer, &hdr_size) == true)
        {
            proxy_transport->send(rewite_header_buffer, hdr_size);
            printf("\033[36mProxy request sent:\033[0m\n%s", rewite_header_buffer);

            http_response_t* proxy_response = new http_response_t;
            const char* parse_proxy_response = proxy_response->parse_header(&reader);

            if (parse_proxy_response) 
            {
                if (parse_proxy_response == reader.error_socket) {
                    printf("proxy_loop() error reading from socket\n");
                    break;
                }
                response->_content_type = proxy_response->_content_type;
                response->_content_lenght = proxy_response->_content_lenght;
                if (*parse_proxy_response != '\n')
                    printf("Remoted end: %s\n", parse_proxy_response);
                else {
                    int rest = proxy_response->_content_lenght;
                    char buffer[4096];

                    response->_header_size = response->prepare_header(buffer, proxy_response->_code, rest);
                    session->get_transport()->send(buffer, response->_header_size);

                    bool sync;
                    char* dst;
                    do {
                        dst = buffer;
                        do {
                            src = reader.get_pchar(sync, rest);
                            if (sync)
                                break;
                            *dst++ = *src;
                            rest--;
                        } while (dst - buffer < sizeof(buffer) - 1
#if LOG_PROXY_RESPOBSE
                            && *src != '\n'
#endif
                            );
                        *dst = 0;
#if ! LOG_PROXY_RESPOBSE
                        session->get_multiart_reader()->get_transport()->send(buffer, (int)(dst - buffer));
#else
                        printf("%s", buffer);
#endif
                        if (sync) {
                            if (src == reader.zero_socket) {
                                break;
                            }
                        }
                    } while ((!proxy_response->_content_lenght || rest > 0) && src != reader.zero_socket);
                }
            }
            else {
                printf("Proxy response not parsed for:   %s:%d\n", request->host->hostname.c_str(), request->host->port);
                break;
            }
        }
        src = request->parse_request(&reader);

    } while (src != reader.error_socket);
}

void proxy_action::process_req(https_session_t* session)
{
    url_t* url = &session->request.url;
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
///    url->rest = url->path + 1;
    printf(" - - - - - - - - - - ACTION: proxy [%s <- %s]\n", url->path, url->path);

        if (url->method == GET)
        {
            char buffer[4096];
            response->_code = 200;
            response->_content_type = "text/html; charset=utf-8";
            response->_body = buffer; 
            response->_body_size = snprintf(buffer, sizeof(buffer),szProxyScreen);;
            response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);
            session->send_prepared_response();
            return;
        }
        else if (url->method == POST)
        {
            int bound_size;
            multipart_stream_reader_t* reader = session->get_multiart_reader();
            if (reader->get_state(bound_size) == multipart_stream_reader_t::Bound)
            {
                reader->set_content_length(request->_content_lenght - bound_size - 2);
                load_form(session);
            }
            else
            {
                if(request->_content_lenght > 0)
                    reader->set_content_length(request->_content_lenght);
                bool sync = false;
                int rest;
                const char *pchar = reader->get_pchar(sync, rest);
                if (reader->get_state(rest) == multipart_stream_reader_t::Bound)
                    load_form(session);
                else
                    printf("\033[36m%s\033[0m\n", pchar);
            }
        }

        form_data_t* form = request->form_data;
        for (section_t* section : form->_objects) {
            if(section->name == "url")
                destination = section->value;
        }

        if (destination.empty()) {
            session->page_not_found(url->method, url->path, session->request._referer.c_str());
            return;
        }

#if true
        request->url.parse(destination.c_str());
#else
        // Тут ещё предстоить парсить строку для перехода
        url_t go_url;
        int pos = destination.find("http://");
        std::string  rq;
        if (pos == 0) {
            rq = destination.substr(7);
        }
        else {
            rq = destination;

        }

        xameleon::get_http_path((char*)rq.c_str(), &go_url);

        if (proxy_transport)
            throw "Proxy transport already defined";
#endif

        url->method = GET;
        if (url->domainname != nullptr)
        {
            request->host->hostname = url->domainname;
            request->host->port = url->port;

            proxy_transport = get_http_transport("proxy-redirector");

            if (proxy_transport->connect(request->host->hostname.c_str(), request->host->port) >= 0)
            {
                this->proxy_loop(session);
            }

            proxy_transport->close();
            release_http_transport(proxy_transport);
            proxy_transport = nullptr;
        }
        else
            fprintf(stderr, "No url for proxy connection\n");
}

