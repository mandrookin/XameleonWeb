#include <string.h>
#include <sstream>
// 中国

#include "../../action.h"
#include "../../lib.h"
#include "../../session.h"
#include "../../session_mgr.h"

extern session_mgr_t* get_active_sessions();

static char* const do_sessions(http_request_t* request, int* response_size)
{
    std::stringstream my_ss(std::stringstream::out);
    session_mgr_t* sessions = get_active_sessions();
    for (auto const& ip : sessions->active_sessions) {
        for (auto const& session : *ip.second) {
            char str[155];
            session.second->get_transport()->describe(str, 155);
            my_ss << str << "  <- " << request->_cookies["lid"] << "<-" << request->_cache_control << "<br>\n";
        }
    }
    *response_size = my_ss.str().size();
    char* p = new char[*response_size + 1];
    strcpy(p, my_ss.str().c_str());
    return (char*) p;
}

http_response_t* admin_action::process_req(https_session_t* session, url_t* url)
{
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
    const char* req_name;
    switch (url->method) {
    case POST:
        req_name = "POST";
        break;
    case GET:
        req_name = "GET";
        break;
    case PUT:
        req_name = "PUT";
        break;
    case PATCH:
        req_name = "PATCH";
        break;
    case DEL:
        req_name = "DEL";
        break;
    default:
        req_name = "not-parsed-";
        break;
    }
    printf("HTTP-%s: Path:%s\n", req_name, url->rest);

    switch (hash(url->rest))
    {
    case hash("sessions"):
        response->_body = do_sessions(request, &response->_body_size);
        break;
    case hash("admin_header.html"):
        response->_body = alloc_file("pages/admin/admin_header.html", &response->_body_size);
        break;
    default:
        response->_body = alloc_file("pages/admin/default.html", &response->_body_size);
        break;
    }

    response->_code = 200;
    response->content_type = "text/html; charset=utf-8";
    response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);

    return response;
}