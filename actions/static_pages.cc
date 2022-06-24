#include <unistd.h>
#include <fcntl.h>
#include <cstring>


#include "../session.h"
#include "../action.h"
#include "../lib.h"

using namespace xameleon;

void xameleon::prepare_file(
    https_session_t* session,
    url_t* url)
{
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
    char  filename[512];

    if (strcmp(url->path, "/") != 0)
        snprintf(filename, 512, "%s%s", session->get_html_root(), url->path);
    else
        snprintf(filename, 512, "%sindex.html", session->get_html_root());

    response->_code = 200;
    response->_body = alloc_file(filename, &response->_body_size);

#if false
    printf("Filename %s\n", filename);
    char* last = strchr(url->rest, '.');
#else
    char* last = strchr(filename, '.');
#endif
    if (last == nullptr)
        response->content_type = "application/octet-stream";
    else {
        last++;
        switch (hash(last))
        {
        case hash("html"):
            response->content_type = "text/html; charset=utf-8";
            break;
        case hash("svg"):
            response->content_type = "image/svg+xml";
            break;
        case hash("jpg"):
            response->content_type = "image/jpg";
            break;
        case hash("png"):
            response->content_type = "image/img";
            break;
        case hash("gif"):
            response->content_type = "image/gif";
            break;
        case hash("bmp"):
            response->content_type = "image/bmp";
            break;
        case hash("xml"):
            response->content_type = "application/xml";
            break;
        default:
            response->content_type = "application/octet-stream";
            break;
        }
    }
    if (response->_body == nullptr) {
        response->content_type = "text/html; charset=utf-8";
        session->page_not_found(GET, url->path, request->_referer.c_str());
    }
}

http_response_t* static_page_action::process_req(
    https_session_t* session,
    url_t* url)
{
    http_response_t* response = &session->response_holder;

    if (url->path[0] == '/' && url->path[1] == 0) {
        strcpy(url->path, "/index.html");
        url->rest = url->path + 1;
    }

    prepare_file(session, url);
    response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);

    printf("ACTION 'static page': %s%s <- %s\n", session->get_html_root(), url->path, url->rest);
    return response;
}

http_response_t* get_favicon_action::process_req(
    https_session_t* session,
    url_t* url)
{
    http_response_t* response = &session->response_holder;
    url->rest = url->path + 1;
    printf("ACTION: GET favicon.ico [%s <- %s]\n", url->path, url->rest);
    prepare_file(session, url);
    response->etag = "favicon.ico";
    response->_max_age = 300000;
    response->content_type = "image/x-icon";
    response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);
    return response;
}


http_response_t* get_touch_action::process_req(https_session_t* session, url_t* url)
{
    http_response_t* response = &session->response_holder;
    bool found = false;

    response->_code = 200;

    printf("Getting secrets for %s:\n", url->rest);
    std::string id = url->rest;
    for (int i = 0; i < url->query_count; i++) {
        printf("  '%s': %s\n", url->query[i].key, url->query[i].val);
        if (strcmp(url->query[i].key, "url") == 0) {
            strcpy(url->path, url->query[i].val);
            // Здесь пока поломано.
            response->add_cookie("lid", id.c_str(), 31536000);
            response->_header_size = response->redirect_to(308, session->request._referer.c_str(), true);
            found = true;
        }
    }
    if (!found) {
        strcpy(url->path, "/notexistpage.html");
        url->rest = url->path + 1;
        prepare_file(session, url);
    }

    return response;
}


