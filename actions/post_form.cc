#include "../action.h"
#include "../multipart_form.h"

// 中国

http_response_t * post_form_action::process_req(https_session_t * session, url_t * url)
{
    http_request_t      *   request = &session->request;
    http_response_t     *   response = &session->response_holder;

#if false
    const int   BUFSIZE = 32 * 1024;
    int                     rest_size = request->_content_lenght;
    char                    buf[BUFSIZE];

    FILE * fp = fopen("post.bin", "wb");

//    fprintf(fp, "%s !\n\n", request->_content_type.c_str());
    while (rest_size > 0) {
        int rd_len = session->get_transport()->recv(buf, BUFSIZE);
        if (rd_len <= 0) {
            printf("Some problem on load POST form.\n");
            break;
        }
        printf("-----   Got data block %u\n", rd_len);
        rest_size -= rd_len; // Это должно быть выше цикла, потому что rd_len изменяется ниже
        fwrite(buf, rd_len, 1, fp);
    }
    fclose(fp);
#else
    if (request->_content_type.rfind("multipart/form-data; boundary=", 0) == 0) {
        const char * bound = request->_content_type.c_str() + 30;

//        printf("BOUNDARY: '%s'\n", bound);
        std::string wider_bound = "\r\n--";
        wider_bound += bound;
        wider_bound += "\r\n";
        form_data_t* data = new form_data_t;
        int status = parse_form_data(session->get_transport(), request->_content_lenght, wider_bound.c_str(), data);
        if (status < 0) {
            char    remote_transport[64];
            session->get_transport()->describe(remote_transport, sizeof(remote_transport));
            fprintf(stderr, "Unable parse form data from %s\n", remote_transport);
        }
        show(data);
        delete data;
    }
#endif
//    response->_header_size = response->redirect_to(303, "/index.html");

    return response;

//rcv_error:
}