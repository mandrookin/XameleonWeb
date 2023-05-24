#ifndef _SERV_H_
#define _SERV_H_

namespace xameleon 
{
    constexpr unsigned int hash(const char* s, int off = 0) {
        return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
    }

    typedef enum {
        EMPTY = 0,
        HTTP = hash("http"),
        HTTPS = hash("https"),
        FTP = hash("ftp")
    } utl_scheme_t;

    typedef enum {
        ERR = -1,
        POST = 0,
        GET,
        PUT,
        PATCH,
        DEL
    } http_method_t;

    typedef enum {
        v0_9,
        v1_0,
        v1_1,
        v2_0
    } http_version_t;

    typedef union {
        http_version_t      human;
        struct {
            short           major;
            short           minor;
        };
    } http_hr_version_t;

    const int max_path_len = 1024;

    typedef struct {
        char* key;
        char* val;
    } key_val_t;

    typedef struct url_s {
        http_method_t       method;
        http_hr_version_t   version;
        utl_scheme_t        scheme;
        int                 port;
        int                 path_len;
        char*               domainname;
        char*               pagename;
        int                 parameters_count;
        key_val_t           parameters[32];
        char*               anchor;
        char                path[max_path_len];
        const char*         get_method();
        const char*         get_http_version();
        bool                parse(const char* url);
        url_s():pagename(nullptr) {}
    } url_t;

    const char* get_http_path(const char* ptr, url_t* uri);
}
#endif
