﻿#include <dirent.h>
#include <sys/stat.h>
#include <list>
#include <string.h>

#include "../action.h"
#include "../lib/utf8_len.h"

using namespace xameleon;

http_response_t * get_directory_action::process_req(https_session_t * session)
{
    url_t* url = &session->request.url;
    http_response_t     *   response = &session->response_holder;
    struct stat path_stat;
    DIR *d;
    struct dirent *dir;
    char  name[512];

    snprintf(name, 512, "%s/%s", session->get_html_root(), (char*) url->path);
    stat(name, &path_stat);
    do {
        if (!S_ISDIR(path_stat.st_mode) || (d = opendir(name)) == nullptr)
        {
            strcpy(url->path, "/notexistpage.html");
            url->rest = url->path + 1;
            response_send_file(session);
            continue;
        }

        response->_body = new char[32 * 1024];
        response->_body_size = 0;

        std::list<std::string> names;

        while ((dir = readdir(d)) != NULL)
            names.push_back(dir->d_name);

        names.sort();

        for (auto item : names)
        {
            snprintf(name, 512, "%s%s/%s", session->get_html_root(), url->path, item.c_str());
            stat(name, &path_stat);
            switch (path_stat.st_mode & S_IFMT) {
            case S_IFREG:
                snprintf(name, 512, "%6u", (unsigned)path_stat.st_size);
                break;
            case S_IFSOCK:
                strcpy(name, "-sock-");
                break;
            case S_IFLNK:
                strcpy(name, "@link@");
                break;
            case S_IFBLK:
                strcpy(name, "-block-");
                break;
            case S_IFDIR:
                strcpy(name, "<dir>");
                break;
            case S_IFCHR:
                strcpy(name, "-chr-");
                break;
            case S_IFIFO:
                strcpy(name, "-fifo-");
                break;
            }
            int tab = 40 - count_utf8_code_points(item.c_str());
            // printf("%s %*c %8s\n", item.c_str(), tab, '\0', name);
            if (response->_body_size < 31 * 1024) { 
                response->_body_size += snprintf(response->_body + response->_body_size, 1024, "%s %*c %9s\n", item.c_str(), tab, ' ', name);
            }

        }
        closedir(d);
        response->_content_type = "text/plain; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);

    } while (false);

    return response;
}
