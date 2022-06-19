#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

// 中国

#include "../action.h"

http_response_t * cgi_action::process_req(https_session_t * session, url_t * url)
{
    http_request_t      *   request = &session->request;
    http_response_t     *   response = &session->response_holder;
    int status;
    struct stat path_stat;
    const int BUFSIZE = 512;
    const int MAXRESPONSE = 64 * 1024;
    FILE *fp;
    char  buff[BUFSIZE];

    printf("ACTION: CGI [%s%s <- %s]\nHeader size = %d url->query_count = %d\n", 
        session->get_html_root(), url->path, url->rest, response->_header_size, url->query_count );

    snprintf(buff, BUFSIZE, "%s%s", session->get_html_root(), (char*)url->path);

    std::string path = getenv("PATH");
    clearenv();

    if (url->query_count != 0) {
        std::string query_recoinstruct;
        for (int i = 0; i < url->query_count; i++) {
            query_recoinstruct += url->query[i].key;
            query_recoinstruct += '=';
            query_recoinstruct += url->query[i].val;
            if (i + 1 < url->query_count)
                query_recoinstruct += '&';
        }
        setenv("QUERY_STRING", query_recoinstruct.c_str(), false);
    }

    do {

        status = stat(buff, &path_stat);
        if (status || !S_ISREG(path_stat.st_mode) || (path_stat.st_mode & S_IXUSR) == 0) {
            printf("Executable %s not found\n", buff);
            session->page_not_found(GET, url->path, request->_referer.c_str());
            continue;
        }

        setenv("PATH", path.c_str(), false);
        setenv("REQUEST_METHOD", 
            url->method ==  GET ? "GET" : 
            url->method == POST ? "POST" :
            url->method == PUT ? "PUT" :
            url->method == PATCH ? "PATCH" :
            url->method == DEL ? "DEL" : "'Unparsed'"
            , false);
        setenv("SCRIPT_NAME", url->path, false);
        setenv("CONTENT_LENGTH", std::to_string(request->_content_lenght).c_str(), false);
        setenv("PATH", path.c_str(), false);

#if POPEN
        if ((fp = popen(buff, "r")) == NULL) {
            session->page_not_found(GET, url->path, request->_referer.c_str());
            continue;
        }
#else   
        // Решение: http://unixwiz.net/techtips/remap-pipe-fds.html
        int	writepipe[2] = { -1,-1 },	/* parent -> child */
            readpipe[2] = { -1,-1 };	/* child -> parent */
        pid_t	childpid;

        if (pipe(readpipe) < 0 || pipe(writepipe) < 0)
        {
            perror("Cannot create pipe for CGI");
            puts("/* close readpipe[0] & [1] if necessary */");
            continue;
        }


#define	PARENT_READ	readpipe[0]
#define	CHILD_WRITE	readpipe[1]
#define CHILD_READ	writepipe[0]
#define PARENT_WRITE	writepipe[1]

        if ((childpid = fork()) < 0)
        {
            perror("Cannot fork child");
            continue;
        }
        else if (childpid == 0)	/* in the child */
        {
            close(PARENT_WRITE);
            close(PARENT_READ);

            dup2(CHILD_READ, 0);  close(CHILD_READ);
            dup2(CHILD_WRITE, 1);  close(CHILD_WRITE);

            /* do child stuff */
            extern char** environ;
            char* argv[2];
            argv[1] = nullptr;
            argv[0] = buff;
            execve(buff, argv, environ);
        }
        else				/* in the parent */
        {
            close(CHILD_READ);
            close(CHILD_WRITE);

            /* do parent stuff */
            fp = fdopen(PARENT_READ, "rb");

            FILE* ofp = fdopen(PARENT_WRITE, "wb");
            if (!ofp) {
                perror("Unable fdopen write pipe. TODO: Check resoiurces leak here");
                continue;
            }
            int l = fwrite((const void*)request->body, 1, (size_t)request->_content_lenght, ofp);
            fclose(ofp);
//            printf("#### %d -> %d\n", request->_content_lenght, l);
        }
#endif

        response->_body = new char[MAXRESPONSE];
        response->_body_size = 0;

        while (fgets(buff, BUFSIZE, fp) != NULL) {
            //printf("OUTPUT: %s", buff);
            int l = strlen(buff);
            if (response->_body_size + l > MAXRESPONSE)
                break;
            memcpy(response->_body + response->_body_size, buff, l);
            response->_body_size += l;
        }

        if (pclose(fp)) {
            printf("Command not found or exited with error status\n");
            session->page_not_found(GET, url->path, request->_referer.c_str());
            continue;
        }

        response->content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);

    } while (false);

    return response;
}