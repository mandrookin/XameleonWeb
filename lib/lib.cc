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
    const char* http_method(http_method_t method) {
        return
            method == POST ? "POST" :
            method == GET ? "GET" :
            method == PUT ? "PUT" :
            method == PATCH ? "PATCH" :
            method == DEL ? "DEL" : "-Unknown-";
    }

    // allocate file once and forever?
char* alloc_file(const char* filename, int* size)
{
//    return nullptr;
    int status;
    struct stat path_stat;
    char * ptr;
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
    ptr = new char[path_stat.st_size];   // char(path_stat.st_size); // 
    *size = (int) path_stat.st_size;
    int sz = read(fd, ptr, path_stat.st_size);
    if (sz != path_stat.st_size) {
        delete []ptr;
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
            default:
                printf("detect %s as application/octet-stream\n", ext);
            }
        }
        return "application/octet-stream";
    }

    int response_send_file(https_session_t* session, const char * filename)
    {
        int status;
        struct stat path_stat;
        http_request_t* request = &session->request;
        http_response_t* response = &session->response_holder;
        transport_i* transport = session->get_transport();
        char buffer[1024 * 16];

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
                    if (status != sz) {
                        printf("Unable send %s file. Connection lost?\n", filename);
                        break;
                    }
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
            return status;

        } while (false);


        if (response->_code != 200) {
            session->page_not_found(GET, request->url.path, request->_referer.c_str());
        }
        return status;
    }

    int response_send_file(https_session_t* session)
    {
        url_t* url = &session->request.url;
        char filename[512];

        if (strcmp(url->path, "/") != 0)
            snprintf(filename, 512, "%s%s", session->get_html_root(), url->path);
        else
            snprintf(filename, 512, "%sindex.html", session->get_html_root());

        return response_send_file(session, filename);
    }
}
