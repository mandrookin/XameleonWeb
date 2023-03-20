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

#include "../session.h"

namespace xameleon
{

    // allocate file once and forever?
char* alloc_file(const char* filename, int* size)
{
    int status;
    struct stat path_stat;
    char* ptr;
    status = stat(filename, &path_stat);
    if (status < 0) {
        printf("HTTP GET: file not found:%s\n", filename);
        return nullptr;
    }
    if (!S_ISREG(path_stat.st_mode)) {
        fprintf(stderr, "Only regular files can be send. Wrong routes settings.\n");
        return nullptr;
    }
    int fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0) {
        printf("Unable open file: %s\n", filename);
        return nullptr;
    }
    ptr = new char[path_stat.st_size];
    *size = path_stat.st_size;
    int sz = read(fd, ptr, path_stat.st_size);
    if (sz != path_stat.st_size) {
        delete ptr;
        *size = 0;
        ptr = nullptr;
    }
    close(fd);
    return ptr;
}

    inline const char* detect_content_type(const char* ext)
    {
        if (ext != nullptr) {
            ext++;
            switch (hash(ext)) {
            case hash("html"):
                return "text/html; charset=utf-8";
                break;
            case hash("svg"):
                return "image/svg+xml";
                break;
            case hash("jpg"):
                return "image/jpg";
                break;
            case hash("png"):
                return "image/img";
                break;
            case hash("gif"):
                return "image/gif";
                break;
            case hash("bmp"):
                return "image/bmp";
                break;
            case hash("xml"):
                return "application/xml";
                break;
            }
        }
        return "application/octet-stream";
    }

    void response_send_file(https_session_t* session)
    {
        int status;
        struct stat path_stat;
        http_request_t* request = &session->request;
        http_response_t* response = &session->response_holder;
        url_t* url = &session->request.url;
        transport_t* transport = session->get_transport();
        char filename[512];
        char buffer[1024 * 16];

        if (strcmp(url->path, "/") != 0)
            snprintf(filename, 512, "%s%s", session->get_html_root(), url->path);
        else
            snprintf(filename, 512, "%sindex.html", session->get_html_root());

        response->_code = 404;
        do {
            status = stat(filename, &path_stat);
            if (status < 0) {
                printf("HTTP GET: file not found:%s\n", filename);
                break;
            }
            if (!S_ISREG(path_stat.st_mode)) {
                fprintf(stderr, "Only regular files can be send. Wrong routes settings.\n");
                break;
            }
            int fd = open(filename, O_RDONLY | O_BINARY);
            if (fd < 0) {
                printf("Unable open file: %s\n", filename);
                break;;
            }

            response->_content_type = detect_content_type(strrchr(filename, '.'));
            response->_code = 200;
            response->_content_lenght = path_stat.st_size;
            response->_header_size = response->prepare_header(buffer, response->_code, response->_content_lenght);
            status = transport->send(buffer, response->_header_size);

            while (true) {
                int sz = read(fd, buffer, sizeof(buffer));
                if (sz > 0) {
                    status = transport->send(buffer, sz);
                    response->_content_lenght -= status;
                    //                printf("  Chunk sent: %d (todo %d)\n", status, response->_content_lenght);
                    continue;
                }
                break;
            }
            close(fd);
            if (response->_content_lenght != 0)
                printf("  '%s' error: %d bytes loosed\n", filename, response->_content_lenght);

            response->_header_size = 0;
            return;
        } while (false);


        if (response->_code != 200) {
            session->page_not_found(GET, url->path, request->_referer.c_str());
            session->counters.not_found++;
            response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);
            int szh = transport->send(response->_header, response->_header_size);
            int szb = transport->send(response->_body, response->_body_size);
            if (szh != response->_header_size || szb != response->_body_size)
                printf("Unable send %s\n", filename);
            delete response->_body;
        }
    }
}
