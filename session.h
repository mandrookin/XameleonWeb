#pragma once

#include <map>
#include <string>

#include "http.h"
#include "transport.h"
#include "counters.h"

namespace xameleon {

    constexpr unsigned int hash(const char* s, int off = 0) {
        return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
    }

    typedef struct {
        std::string     value;
        unsigned int    lifetime;
    } cookie_t;

    typedef struct {
        int     start;
        int     stop;
    } range_t;

    typedef enum {
        plain = 0,
        gzip = 1,
        deflate = 2,
        br = 4
    } encoding_t;

    class http_request_t {
        static const unsigned max_range_count = 3;
        void parse_cookies(char* cookie);
        void parse_range(char* ranges);
        void parse_cache_control(char* cache_control);
        void parse_encoding(char* enc);
        void prepare();
    public:
        std::map<std::string, std::string>  _cookies;
        std::string _host;
        unsigned short port;
        std::string _referer;
        std::string _content_type;
        std::string _cache_control;
        int _content_lenght;
        encoding_t  encoding;
        unsigned int ranges_count;
        range_t     ranges[max_range_count];

        char* parse_http_header(char* header);
        char* body;
    };

    class http_response_t {
        int add_cookies_to_header(char* buffer, int buffsz);
    public:
        unsigned int    _max_age;
        char* _body;
        int             _body_size;
        int             _header_size;
        int             _code;

        std::map<std::string, cookie_t>  _cookies;
        std::string etag;
        std::string content_type;

        char  _header[2048];

        void add_cookie(const char* name, const char* value, unsigned int seconds = 0);
        int prepare_header(char* header, int code, int body_size);
        int redirect_to(int code, const char* url, bool keep_alive);
        void clear();
        void release();

        http_response_t() {
            _max_age = 0;
            _header_size = 0;
            _body_size = 0;
            _body = nullptr;
        }
    };

    class https_session_t {
        transport_i* transport;
        std::string                 homedir;
        http_session_counters_t     counters;
    public:
        time_t              start_time;
        bool                cookie_found;
        bool                session_found;
        http_request_t      request;
        http_response_t     response_holder;


    private:
        void log(url_t& url, char* request_body);

    public:
        http_response_t* page_not_found(http_method_t method, char* url, const char* referer);

        https_session_t(transport_i* transport, const char* homedir) :
            transport(transport),
            homedir(homedir),
            start_time(time(nullptr)) {}
        transport_i* get_transport() { return transport; }
        const char* get_html_root() { return homedir.c_str(); }
        http_session_counters_t get_counters() { return counters; }
        void* https_session();
    };
}

extern void generate_uuid_v4(char * guid);
