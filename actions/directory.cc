#define _CRT_SECURE_NO_WARNINGS 1

#include <sys/stat.h>
#include <list>
#include <string.h>

#include "../stream_writer.h"

#ifndef _WIN32
#include <dirent.h>
#include "../lib/utf8_len.h"
#else
#include <io.h>
#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

#define DIR WIN32_FIND_DATA
#endif

#endif

#include "../action.h"

using namespace xameleon;

void get_directory_action::process_req(https_session_t * session)
{
    int namelen, counter = 0;
    url_t* url = &session->request.url;
    http_response_t     *   response = &session->response_holder;
    struct stat path_stat;
    stream_writer   sender(session);
#ifndef _WIN32
    DIR *d;
    struct dirent* dir;
    char referer[1024];
#endif
    char  name[512];


    namelen = snprintf(name, 512, "%s/%s", session->get_html_root(), (char*) url->path);
    stat(name, &path_stat);
    do {
        const char* ref_from = session->request._referer.c_str();
        
        if (!S_ISDIR(path_stat.st_mode)) {
            session->page_not_found(GET, name, ref_from);
            return;
        }

        response->_content_type = "text/plain; charset=utf-8";

#ifndef _WIN32 
        if ((d = opendir(name)) == nullptr) {
            response_send_file(session, "/havenorights.html");
            return;
        }
        response->_body = new char[32 * 1024];
        response->_body_size = 0;

        std::list<std::string> names;

        while ((dir = readdir(d)) != NULL)
            names.push_back(dir->d_name);

        names.sort();

#if ! OLD
        response->_header_size = response->prepare_header(response->_header, 200, 0);
        sender.send(false, response->_header, response->_header_size);
#endif

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
#if OLD
            if (response->_body_size < 31 * 1024) {
                response->_body_size += snprintf(response->_body + response->_body_size, 1024, "%s %*c %9s\n", item.c_str(), tab, ' ', name);
            }
#else
            sender.sprintf(false, "%s %*c %9s\n", item.c_str(), tab, ' ', name);
#endif

        }
        closedir(d);
#if OLD
        response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);
        session->send_prepared_response();
#else
        sender.send(true, 0, 0);
        sender.close_on_end();
#endif

#else
        response->_header_size = response->prepare_header(response->_header, 200, 0);
        sender.send(false, response->_header, response->_header_size);

        HANDLE hFind = (HANDLE)-1;
        DIR ffd;
        LARGE_INTEGER filesize;

        strncpy(name + namelen, "/*", sizeof(namelen));

        hFind = ::FindFirstFile( name, &ffd);
        if (INVALID_HANDLE_VALUE == hFind)
        {
            response_send_file(session, "/havenorights.html");
            return;
        }
        do
        {
            counter++;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if(ffd.cFileName[0] != '.')
                    sender.sprintf(false, "  %s   <DIR>\n", ffd.cFileName);
            }
            else
            {
                filesize.LowPart = ffd.nFileSizeLow;
                filesize.HighPart = ffd.nFileSizeHigh;
                sender.sprintf(false, "%s\t\t%ld bytes\n", ffd.cFileName, filesize.QuadPart);
            }
        } while (FindNextFile(hFind, &ffd) != 0);
        FindClose(hFind);
        sender.send(true, 0, 0);
        sender.close_on_end();

        return;
#endif
    } while (false);
}
    