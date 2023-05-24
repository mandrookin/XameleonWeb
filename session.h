#pragma once

#include <map>
#include <string>

#ifdef _WIN32
#include <time.h>
#endif

#include "./http.h"
#include "hosts.h"
#include "transport.h"
#include "counters.h"
#include "lib/multipart_stream_reader.h"
#include "multipart_form.h"

namespace xameleon {

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

    typedef enum { 
        text = hash("text"),
        image = hash("image"),
        multipart = hash("multipart")
    } major_type_t;

    typedef enum {
        // multipart
        formdata = hash("form-data"),
        related = hash("related"),
        mixed = hash("mixed"),
        // text
        html = hash("html"),
        css = hash("css"),
        // image
        gif = hash("gif"),
        png = hash("png"),
        jpg = hash("jpg"),
    } minor_type_t;

    typedef enum {
        notype = 0,
        keepalive = hash("keep-alive"),
    } connection_type_t;

    class http_request_t {
        typedef enum { undefined, first_split, second_arg, lf } context_state_t;
        major_type_t        major_type;
        minor_type_t        minor_type;
        context_state_t     context_state;
        connection_type_t   connection_type;
        int                 upgrade_insecure_requests;

        static const unsigned max_range_count = 3;
        // Переделать на const char *
        void parse_cookies(char* cookie);
        void parse_range(char* ranges);
        void parse_cache_control(char* cache_control);
        void parse_accept_formats(char* cache_control);
        void parse_encoding(char* enc);
        void parse_forward_tag(char* ip_string);
        void parse_connection(char* connection_option);
        void prepare();
        char* parse_content_type(char* value);
        const char* parse_http_header(http_stream_t* reader);
    public:
        url_t                               url;
        std::map<std::string, std::string>  _cookies;
        std::map<std::string, std::string>  _collected_tags;
//        std::string                         _host;
        host_t*                             host;
//        unsigned short                      port;
        std::string                         _referer;
        std::string                         _content_type;
        std::string                         _cache_control;
        std::string                         _user_agent;
        std::string                         _accept;
        int                                 _content_lenght;
        encoding_t                          encoding;
        unsigned int                        ranges_count;
        range_t                             ranges[max_range_count];
        uint32_t                            _x_forward_for;
        uint32_t                            _x_real_ip;
        form_data_t*                        form_data;

        const char* parse_header(http_stream_t* reader);
        const char* parse_request(http_stream_t* reader);
        const char* body;

        // Input - buffer size, output - generated header size
        bool proxy_rewrite_header(char* header_buffer, int * size);

        http_request_t() { 
            host = nullptr;
            form_data = nullptr;
            context_state = undefined;
            connection_type = notype;
            upgrade_insecure_requests = false;
            _x_forward_for = 0;
            _x_real_ip = 0;
        }
        ~http_request_t() { 
            if (form_data) delete form_data; 
        }
    };

    class http_response_t {
        int add_cookies_to_header(char* buffer, int buffsz);
    public:
        unsigned int    _max_age;
        const char*           _body;
        int             _body_size;
        int             _header_size;
        int             _code;

        std::map<std::string, cookie_t>  _cookies;
        std::string                         _server;
        std::string                         _location;
        std::string                         _date;  // Переделать в datetime
        std::string                         _connection;
///////        std::string                         etag;
        std::string                         _content_type;
        std::string                         _cache_control;
        std::string                         _x_frame_options;
        std::string                         _content_security_policy;
        int                                 _content_lenght;
        std::string                         _http_reason;
        std::string                         _accept_ranges;
///////        std::string                         _cache_control;
        std::string                         _etag;

        char  _header[2048];

        void add_cookie(const char* name, const char* value, unsigned int seconds = 0);
        int prepare_header(char* header, int code, int body_size);
        int redirect_to(int code, const char* url, bool keep_alive);
        void clear();
        void release();

        const char* parse_header_option(const char* name, const char* value);
        const char* parse_header(http_stream_t* reader);

        http_response_t() {
            _max_age = 0;
            _header_size = 0;
            _body_size = 0;
            _body = nullptr;
        }
    };

    class https_session_t {
        multipart_stream_reader_t*  reader;
    public:
        time_t                      start_time;
        http_session_counters_t     counters;
        bool                        cookie_found;
        bool                        session_found;
        http_request_t              request;
        http_response_t             response_holder;


    private:
        void log(char* request_body);
        int tracky_response(url_t* url);
        void reverse_proxy();

    public:
        void page_not_found(http_method_t method, const char* url, const char* referer);
        void site_not_found(const char* reqested_host);

        https_session_t(transport_i* transport) :
            start_time(time(nullptr)) 
        {
            cookie_found = false;
            session_found = false;
            reader = new multipart_stream_reader_t(transport);
        }
        ~https_session_t() 
        {
            transport_i* fp = reader->get_transport();
            if (fp) {
                delete fp;
            }
            delete reader;
            reader = nullptr;
        }
        transport_i* get_transport() { 
            return this->reader->get_transport(); 
        }
        multipart_stream_reader_t* get_multiart_reader() { return reader; }
        const char* get_html_root() { 
            return request.host ? request.host->home.c_str() : "pages";
        }
        http_session_counters_t get_counters() { return counters; }
        void* https_session();
        int send_prepared_response() {
            int hsize, bsize;
            hsize = get_transport()->send(response_holder._header, response_holder._header_size);
            if (hsize == response_holder._header_size) {
                bsize = get_transport()->send(response_holder._body, response_holder._body_size);
                if (bsize == response_holder._body_size)
                    return hsize + bsize;
            }
            return -1;
        }
    };

    int response_send_file(class https_session_t* session);
    int response_send_file(class https_session_t* session, const char * filenane);

}

extern void generate_uuid_v4(char * guid);
