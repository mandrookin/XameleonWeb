#pragma once

#include <map>
#include <string>

#ifdef _WIN32
#include <time.h>
#endif

#include "./http.h"
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

    class http_request_t {
        typedef enum { undefined, first_split, second_arg, lf } context_state_t;
        major_type_t        major_type;
        minor_type_t        minor_type;
        context_state_t     context_state;

        static const unsigned max_range_count = 3;
        void parse_cookies(char* cookie);
        void parse_range(char* ranges);
        void parse_cache_control(char* cache_control);
        void parse_encoding(char* enc);
        void parse_forward_tag(char* ip_string);
        void prepare();
        char* parse_content_type(char* value);
        const char* parse_http_header(multipart_stream_reader_t* reader);
    public:
        url_t                               url;
        std::map<std::string, std::string>  _cookies;
        std::string                         _host;
        unsigned short                      port;
        std::string                         _referer;
        std::string                         _content_type;
        std::string                         _cache_control;
        int                                 _content_lenght;
        encoding_t                          encoding;
        unsigned int                        ranges_count;
        range_t                             ranges[max_range_count];
        uint32_t                            x_forward_for;
        uint32_t                            x_real_ip;
        form_data_t*                        form_data;

        const char* parse_header(multipart_stream_reader_t* reader);
        const char* parse_request(multipart_stream_reader_t* reader);
        const char* body;

        // Input - buffer size, output - generated header size
        bool proxy_rewrite_header(char* header_buffer, int * size);

        http_request_t() { 
            form_data = nullptr;
            context_state = undefined;
        }
        ~http_request_t() { 
            if (form_data) delete form_data; 
        }
    };

    class http_response_t {
        int add_cookies_to_header(char* buffer, int buffsz);
    public:
        unsigned int    _max_age;
        char*           _body;
        int             _body_size;
        int             _header_size;
        int             _code;

        std::map<std::string, cookie_t>  _cookies;
        std::string                         _server;
        std::string                         _location;
        std::string                         _date;  // Переделать в datetime
        std::string                         _connection;
        std::string                         etag;
        std::string                         _content_type;
        std::string                         _cache_control;
        std::string                         _x_frame_options;
        std::string                         _content_security_policy;
        int                                 _content_lenght;

        std::string http_reason;

        char  _header[2048];

        void add_cookie(const char* name, const char* value, unsigned int seconds = 0);
        int prepare_header(char* header, int code, int body_size);
        int redirect_to(int code, const char* url, bool keep_alive);
        void clear();
        void release();

        const char* parse_header_option(const char* name, const char* value);
        const char* parse_header(multipart_stream_reader_t* reader);

        http_response_t() {
            _max_age = 0;
            _header_size = 0;
            _body_size = 0;
            _body = nullptr;
        }
    };

    class https_session_t {
        multipart_stream_reader_t*  reader;
        std::string                 homedir;
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

    public:
        http_response_t* page_not_found(http_method_t method, char* url, const char* referer);

        https_session_t(transport_i* transport, const char* homedir) :
            homedir(homedir),
            start_time(time(nullptr)) 
        {
            reader = new multipart_stream_reader_t(transport);
        }
        ~https_session_t() 
        {
            transport_t* fp = reader->get_transport();
            if (fp)
                delete fp;
            delete reader;
        }
        transport_i* get_transport() { 
            return this->reader->get_transport(); 
        }
        multipart_stream_reader_t* get_multiart_reader() { return reader; }
        const char* get_html_root() { return homedir.c_str(); }
        http_session_counters_t get_counters() { return counters; }
        void* https_session();
    };

    void response_send_file(class https_session_t* session);

}

extern void generate_uuid_v4(char * guid);
