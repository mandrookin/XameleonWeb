#include <unistd.h>
#include <fstream>

#include "../action.h"
#include "../multipart_form.h"

using namespace xameleon;

http_response_t * post_form_action::process_req(https_session_t * session)
{
    url_t               *   url = &session->request.url;
    http_request_t      *   request = &session->request;
    http_response_t     *   response = &session->response_holder;
    FILE                *   copy = nullptr;

    std::string holder;

    if (request->_content_type.rfind("multipart/form-data; boundary=", 0) == 0) {
        const char * bound = request->_content_type.c_str() + 30;
 //        printf("BOUNDARY: '%s'\n", bound);

//        std::string wider_bound = "\r\n--";
//        wider_bound += bound;
//        wider_bound += "\r\n";
        bool sync = false;
        form_data_t* data = new form_data_t(bound);
        int status = parse_form_data(session->get_multiart_reader(), data, sync);
        if (status < 0) {
            char    remote_transport[64];
            session->get_transport()->describe(remote_transport, sizeof(remote_transport));
            fprintf(stderr, "Unable parse form data from %s\n", remote_transport);
        }
        show(data);

        char buff[512];
        for (auto const& section : data->_objects) {
            switch(section->type) {
            case region::text:
                holder += "<b>" + section->name + "</b>: " + section->value + "<br/>";
                break;
            case region::file:
                snprintf(buff, sizeof(buff), "pages/upload/%s", section->value.c_str());
#if false // Not works between mounts
                if (link(section->filename.c_str(), buff) < 0)
                    perror("Unable link file");
#elif false // Not compatible with mktemp
                {
                    std::ifstream ifs(section->filename, std::ios::in | std::ios::binary);
                    ifs.seekg(0);
                    std::ofstream ofs(buff, std::ios::out | std::ios::binary);
                    ofs << ifs.rdbuf();
                }
#else
                if (section->fp == nullptr) {
                    fprintf(stderr, "File handle is not prepared\n");
                    continue;
                }
                printf("Copy file %s -> %s\n", section->filename.c_str(), section->value.c_str());

                fflush(section->fp);
                fseek(section->fp, 0, SEEK_SET);
                copy = fopen(buff, "wb");
                if (copy == nullptr) {
                    fprintf(stderr, "Unable create file %s\n", buff);
                    continue;
                }
                char data[65536];
                ssize_t count;
                while(true)
                {
                    count = fread(data, 1, sizeof(data), section->fp);
                    if (count < 0) {
                        fprintf(stderr, "Unable read temporary file\n");
                        break;
                    }
                    if (count == 0)
                        break;
                    fwrite(data, count, 1, copy);
                }
                fclose(copy);
                fclose(section->fp);
                section->fp = nullptr;
#endif
                holder += "Link file '" + section->filename + "' to '/upload/" + section->value + "'<br/>";
                break;
            default:
                holder += "Unparsed section type<br/>";
            }
        }

        delete data;
    }

    response->_content_type = "text/html; charset=utf-8";
    response->_body = new char[1024];
    response->_body_size = snprintf(response->_body, 1024,
        "<html><body>"
        "Всё будет, но <b>не</b> скоро.<br/>"
        "%s"
        "<a href='/'>Вернуться на главную страницу</a>"
        "</body></html>",
        holder.c_str()
        );
    response->_header_size = response->prepare_header(response->_header, 201, response->_body_size);
    return response;
}