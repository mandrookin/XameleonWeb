#pragma once

#include "http.h"
#include "session.h"

namespace xameleon {

    typedef enum {
        guest,      // everybody, Not tracked
        tracked,    // everybody, Traccked
        user,       // registered user (check groups???)
        editor,     // editro rights
        admin       // admin rights
    } access_t;

    class http_action_t {
    protected:
        access_t    rights;
    public:
        virtual http_response_t* process_req(https_session_t* session) = 0;
        virtual ~http_action_t() {};
        http_action_t(access_t level) { rights = level; }
        access_t get_rights() { return rights; }
    };

    int add_action_route(const char* path, http_method_t method, http_action_t* handler);
    http_action_t* find_http_action(url_t& url);

    /* Built-in actions */
    class static_page_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        static_page_action(access_t rights) : http_action_t(rights) {}
    };

    class get_favicon_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        get_favicon_action() : http_action_t(guest) {}
    };

    class get_touch_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        get_touch_action() : http_action_t(guest) {}
    };

    class get_directory_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        get_directory_action(access_t rights) : http_action_t(rights) {}
    };

    class cgi_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        cgi_action(access_t rights) : http_action_t(rights) {}
    };

    class post_form_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        post_form_action(access_t rights) : http_action_t(rights) {}
    };

    class admin_action : public http_action_t {
        http_response_t* process_req(https_session_t* session);
    public:
        admin_action() : http_action_t(admin) {}
    };

    class proxy_action: public http_action_t {
        std::string   destination;
        transport_t* proxy_transport;
        http_response_t* process_req(https_session_t* session);
        int load_form(https_session_t* session);
    public:
        proxy_action() : http_action_t(admin) {
            proxy_transport = nullptr;
        }
    };

}
