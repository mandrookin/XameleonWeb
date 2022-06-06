#include "../action.h"

// 中国

http_response_t * post_form_action::process_req(https_session_t * session, url_t * url)
{
    const int   BUFSIZE = 32 * 1024;
    http_request_t      *   request = &session->request;
    http_response_t     *   response = &session->response_holder;
    int                     rest_size = request->_content_lenght;
    char                    buf[BUFSIZE];

    FILE * fp = fopen("post.bin", "wb");

#if true
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
        printf("BOUNDARY: %s\n", bound);
        int bound_len = strlen(bound);
        int match_rest = bound_len;
        int rd_len;

        while (rest_size > 0) {
            rd_len = SSL_read(ssl, buf, BUFSIZE);
            if (rd_len <= 0)
                goto rcv_error;

            rest_size -= rd_len; // Это должно быть выше цикла, потому что rd_len изменяется ниже
            char * src = buf;

            while (match_rest > 0 && rd_len > 0) {
                idx = bound_len - match_rest;
                if (src[idx] == bound[idx]) {
                    match_rest--;
                    continue;
                }
                match_rest = bound_len;
                src++;
                rd_len--;
            }
            if (rd_len == 0) // А может быть match_rest != 0 ? Да без разницы!
                continue;

            src += bound_len;
            int last = rd_len - (src - buf);
        }
    }
#endif
    response->_header_size = response->redirect_to(303, "/index.html");

    return response;

//rcv_error:
}