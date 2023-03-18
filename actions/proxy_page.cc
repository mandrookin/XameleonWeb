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

http_response_t* proxy_action::process_req(https_session_t* session)
{
    url_t* url = &session->request.url;
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
    url->rest = url->path + 1;
    printf("ACTION: proxy [%s <- %s]\n", url->path, url->rest);

    if (destination.empty())
    {
        if (url->method == GET)
        {
            response->_code = 200;
            response->_content_type = "text/html; charset=utf-8";
            response->_body = new char[4096];
            response->_body_size = snprintf(response->_body, 4096,
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
                "<input type = \"submit\" class=\"button\" value=\" Go \" />"
                "</form>"
                "</body>"
                "</html>\r\n"
            );
            response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);
            return response;
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
            response->_content_type = "text/html; charset=utf-8";
            return session->page_not_found(url->method, url->path, session->request._referer.c_str());
        }

        // Тут ещё предстоить парсить строку для перехода
        url_t go_url;
        xameleon::get_http_path((char*)destination.c_str(), &go_url);

        if (proxy_transport)
            throw "Proxy transport already defined";

        url->method = GET;
        request->_host = destination;
        request->port = 80;


        char rewite_header_buffer[2048];
        int hdr_size = sizeof(rewite_header_buffer);
        if (request->proxy_rewrite_header(rewite_header_buffer, &hdr_size) == true)
        {
            proxy_transport = create_http_transport();
            if (proxy_transport->connect(request->_host.c_str(), request->port) >= 0)
            {
                proxy_transport->send(rewite_header_buffer, hdr_size);
                printf("\033[36mProxy request sent:\033[0m\n%s", rewite_header_buffer);

                multipart_stream_reader_t* reader = new multipart_stream_reader_t(proxy_transport, nullptr);
                http_response_t* proxy_response = new http_response_t;
                const char* parse_proxy_response = proxy_response->parse_header(reader);

                if (parse_proxy_response) {
                    response->_content_type = proxy_response->_content_type;
                    response->_content_lenght = proxy_response->_content_lenght;
                    if (*parse_proxy_response != '\n')
                        printf("Remoted end: %s\n", parse_proxy_response);
                    else {
                        int rest = proxy_response->_content_lenght;
                        char buffer[4096];

                        response->_header_size = response->prepare_header(buffer, proxy_response->_code, rest);
                        session->get_multiart_reader()->get_transport()->send(buffer, response->_header_size);

                        bool sync;
                        const char* src;
                        char* dst;
                        do {
                            dst = buffer;
                            do {
                                src = reader->get_pchar(sync, rest);
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
                            session->get_multiart_reader()->get_transport()->send(buffer, dst-buffer);
#else
                            printf("%s", buffer);
#endif
                            if (sync) {
                                if (src == reader->zero_socket) {
                                    break;
                                }
                            }
                        } while (rest > 0 && src != reader->zero_socket);
                    }
                }
                else
                    printf("Proxy response not parsed for:   %s:%d\n", request->_host.c_str(), request->port);
                proxy_transport->close();
                delete proxy_response;
            }
            delete proxy_transport;
            proxy_transport = nullptr;
        }
destination.clear();
        response->_header_size = 0;
        return response;
            // session->page_not_found(url->method, url->path, session->request._referer.c_str());
    }

    throw "WE DID IT!";


    return response;
}

