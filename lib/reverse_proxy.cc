#include "../session.h"

namespace xameleon {

    void redirect_response(
        http_response_t& response_holder,
        transport_i* proxy_transport,
        http_stream_t& proxy_reader,
        transport_i* client_transport,
        http_response_t& proxy_response)
    {
        //                            int rest = 0;
        int collected_bytes;
        int total_bytes = 0;
        const char* src = nullptr;
        char buffer[16384];

        do {
            response_holder._header_size = proxy_response.prepare_header(buffer, proxy_response._code, proxy_response._content_lenght);
            client_transport->send(buffer, response_holder._header_size);
            ////proxy_transport->send(buffer, response_holder._header_size);
            bool sync = false;
            char* dst;

            printf("Proxy response content length = %d\n", proxy_response._content_lenght);

            int sent_bytes = 0;

            do {
                dst = buffer;
                do {
                    int buffered_chars = 0;
                    src = proxy_reader.get_pchar(sync, buffered_chars);
                    if (sync)
                        break;
                    *dst++ = *src;
                    //                                    rest--;
                    collected_bytes = dst - buffer;
                    total_bytes++;
                } while (collected_bytes < sizeof(buffer) - 1 && total_bytes < proxy_response._content_lenght
#if LOG_PROXY_RESPOBSE
                    && *src != '\n'
#endif
                    );

                int data_size = (int) (dst - buffer);
                if (data_size == 0) {
                    printf("Proxy full response sent\n");
                    break;
                }
                //            data_size = get_multiart_reader()->get_transport()->send(buffer, data_size);

                buffer[data_size] = 0;
//                printf(" ---->>>>>\033[33m\n \n\n%s\033[0m\n", buffer);

                data_size = client_transport->send(buffer, data_size);
                    if (data_size != dst - buffer) {
                    fprintf(stderr, "Unable sent proxy response. Data size %d buffer count %d\n", data_size, (int)(dst - buffer));
                }
                if (total_bytes == proxy_response._content_lenght) {
                    printf("Got full page %d\n", total_bytes);
                    data_size = client_transport->send((char*)"\n", 1);
                    break;
                }
                sent_bytes += data_size;
                fprintf(stderr, "... (%d) %d of %d bytes sent\n", sent_bytes, total_bytes, proxy_response._content_lenght);


                //                                printf("%s", buffer);

                if (sync) {
                    if (src == proxy_reader.zero_socket) {
                        printf("Remote sent zero bytes. Try still loading...\n");
                        continue;
                    }
                    if (src == proxy_reader.error_socket) {
                        break;
                    }
                    printf("What is sync?\n");
                }

            } while (total_bytes < proxy_response._content_lenght);
            *dst = 0;

//            const char* parse_proxy_response = proxy_response.parse_header(&proxy_reader);
            break;

        } while (src != proxy_reader.error_socket && src != proxy_reader.zero_socket);
        printf("Interaction with the destination server is completed.\n");
    }

    void https_session_t::reverse_proxy()
    {
        http_response_t                         proxy_response;
        transport_i* proxy_transport;

        size_t pos = request.host->home.find("://");

        const char* proxy_name = request.host->home.c_str();
        proxy_name = pos == std::string::npos ? proxy_name : proxy_name + pos + 3;

        proxy_transport = get_http_transport("reverse-prozy");

        if (proxy_transport->connect(proxy_name, request.host->port) < 0)
        {
            char buff[4096];
            snprintf(buff, sizeof(buff), "reverse_proxy: Unable connect to host: %s\n", proxy_name);
            this->site_not_found(buff);
            fprintf(stderr, buff);
            release_http_transport(proxy_transport);
            return;
        }

        {
            int size;
            int bytes_sent;
            bool proxy_active = true;

            char entry_stack_protector[16];
            raw_http_stream_t  proxy_reader(proxy_transport);
            char leave_stack_protector[16];

            char    proxy_header_buffer[4096];
//            int collected_bytes, total_bytes;

            do
            {
                entry_stack_protector[0] = 0;
                leave_stack_protector[0] = 0;

                const char* src = nullptr;
                switch (request.url.method)
                {
                case POST:
                case GET:
                case PUT:
                case PATCH:
                case DEL:
                    size = sizeof(proxy_header_buffer);
                    request.proxy_rewrite_header(proxy_header_buffer, &size);
                    bytes_sent = proxy_transport->send(proxy_header_buffer, size);
                    if (size != bytes_sent) {
                        printf("\033[35mRemote host closed connecttion:\033[0m\n");
                        proxy_active = false;
                        break;
                    }

                    {
                        printf("\033[36mProxy request sent:\033[0m\n%s", proxy_header_buffer);
                        const char* parse_proxy_response = proxy_response.parse_header(&proxy_reader);
                        if (parse_proxy_response == reader->error_socket)
                        {
                            fprintf(stderr, "Socket error\n ");
                            proxy_active = false;
                        }
                        else if (parse_proxy_response == nullptr) {
                            fprintf(stderr, "Unable parse proxy_response header\n ");
                        }
                        else if (*parse_proxy_response != '\n')
                            printf("Remoted end: %s\n", parse_proxy_response);
                        else {
                            response_holder._code = proxy_response._code;
                            response_holder._content_type = proxy_response._content_type;
                            response_holder._content_lenght = proxy_response._content_lenght;

                            redirect_response(response_holder, proxy_transport, proxy_reader, get_multiart_reader()->get_transport(), proxy_response);

                            // Без этого клиент продолжает ждать, да и с этим ждёт
                            proxy_active = false;
                        }
                    }
                    break;
                default:
                    proxy_active = false;
                    fprintf(stderr, "Closint connection on unknown HTTP request\n");
                    break;
                }

                if (proxy_active) {
                    reader->reset_buffer();
                    src = request.parse_request(reader);
                }

            } while (proxy_active);
        }

        proxy_transport->close();
        release_http_transport(proxy_transport);
    }

}
