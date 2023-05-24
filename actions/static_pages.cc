#define _CRT_SECURE_NO_WARNINGS 1
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
 #include <unistd.h>
 #define O_BINARY  0
#else
 #include <io.h>
 #if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
  #define open _open
  #define read _read
  #define close _close
 #endif
#endif

#include <fcntl.h>
#include <cstring>


#include "../session.h"
#include "../action.h"
#include "../lib.h"

using namespace xameleon;

void static_page_action::process_req(https_session_t* session)
{
    http_response_t* response = &session->response_holder;
    url_t* url = &session->request.url;

    if (url->path[0] == '/' && url->path[1] == 0) {
        strcpy(url->path, "/index.html");
        url->anchor = url->path + 1;
    }

    printf("ACTION 'static page': %s%s\n", session->get_html_root(), url->pagename);

    response_send_file(session);
}

static char * favicon_buff = nullptr;
static int favicon_size = 0;

void get_favicon_action::process_req(https_session_t* session)
{
    transport_i* transport = session->get_multiart_reader()->get_transport();
    http_request_t*  request  = &session->request;    
    http_response_t* response = &session->response_holder;

    if (!favicon_buff && !favicon_size) {
        char filename[180];
        snprintf(filename, 180, "%s/favicon.ico", session->get_html_root());
        favicon_buff = xameleon::alloc_file(filename, &favicon_size);
        if (!favicon_buff)
            favicon_size = -1;
    }

//    favicon_buff = (char*) "abc";
//    favicon_size = 3;

    if (favicon_buff) {
        response->_etag = "favicon.ico";
        response->_max_age = 300000;
        response->_content_type = "image/x-icon";
        response->_body_size = favicon_size;
        response->_body = favicon_buff;
        response->_code = 200;

        response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);
        int szh = transport->send(response->_header, response->_header_size);
        int szb = transport->send((char*) response->_body, response->_body_size);
        if (szh != response->_header_size || szb != response->_body_size)
            printf("Unable send favicon.ico");
    }
    else {
        session->page_not_found(GET, request->url.path, request->_referer.c_str());
        session->counters.not_found++;
    }
}


void get_touch_action::process_req(https_session_t* session)
{
    http_response_t* response = &session->response_holder;
    url_t& url = session->request.url;
    bool found = false;

    printf("%s secrets for %s:\n", http_method(url.method), url.anchor);
    std::string id = url.anchor;
    for (int i = 0; i < url.parameters_count; i++) {
        printf("  '%s': %s\n", url.parameters[i].key, url.parameters[i].val);
        if (strcmp(url.parameters[i].key, "url") == 0) {
            strcpy(url.path, url.parameters[i].val);
            // Здесь пока поломано.
            response->add_cookie("lid", id.c_str(), 31536000);
            response->_header_size = response->redirect_to(308, session->request._referer.c_str(), true);
            session->get_transport()->send(response->_header, response->_header_size);
            found = true;
            break;
        }
    }
    if (!found)
        session->page_not_found(http_method_t::GET,"--secret--","--seccret--");
}
