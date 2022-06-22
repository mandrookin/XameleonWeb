#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netdb.h>

// 中国

#include "../action.h"

typedef struct env {
    int count;
    int size;
    const char* environ[32];
    char holder[4096];
    env() { for (int i = 0; i < 32; i++) environ[i] = nullptr; count = 0; size = 0; }
    int setenv(const char* name, const char* value);
} env_t;

int env_t::setenv(const char* name, const char* value)
{
    if (count > 31 || (sizeof(holder) - size) < 5) {
        fprintf(stderr, "CGI environment overflow\n");
        return -1;
    }

    int len = snprintf(holder + size, sizeof(holder) - size, "%s=%s", name, value);
    environ[count] = holder + size;
    size += len + 1;
    count++;
    return count;
}

http_response_t * cgi_action::process_req(https_session_t * session, url_t * url)
{
    http_request_t      *   request = &session->request;
    http_response_t     *   response = &session->response_holder;
    env_t                   env;
    int status;
    struct stat path_stat;
    const int BUFSIZE = 512;
    const int MAXRESPONSE = 64 * 1024;
    FILE *fp;
    char  executable[BUFSIZE];
    char  buff[BUFSIZE];

    printf("ACTION: CGI [%s%s <- %s]\nHeader size = %d url->query_count = %d\n", 
        session->get_html_root(), url->path, url->rest, response->_header_size, url->query_count );


    do 
    {
        snprintf(executable, BUFSIZE, "%s%s", session->get_html_root(), (char*)url->path);
        status = stat(executable, &path_stat);
        if (status || !S_ISREG(path_stat.st_mode) || (path_stat.st_mode & S_IXUSR) == 0) {
            printf("Executable %s not found\n", executable);
            session->page_not_found(GET, url->path, request->_referer.c_str());
            continue;
        }

        if (url->query_count != 0) {
            std::string query_recoinstruct;
            for (int i = 0; i < url->query_count; i++) {
                query_recoinstruct += url->query[i].key;
                query_recoinstruct += '=';
                query_recoinstruct += url->query[i].val;
                if (i + 1 < url->query_count)
                    query_recoinstruct += '&';
            }
            env.setenv("QUERY_STRING", query_recoinstruct.c_str());
        }


        env.setenv("REQUEST_METHOD",
            url->method == GET ? "GET" :
            url->method == POST ? "POST" :
            url->method == PUT ? "PUT" :
            url->method == PATCH ? "PATCH" :
            url->method == DEL ? "DEL" : "'Unparsed'");
        env.setenv("SCRIPT_NAME", url->path);
        env.setenv("CONTENT_LENGTH", std::to_string(request->_content_lenght).c_str());
        std::string path = getenv("PATH");
        env.setenv("PATH", path.c_str());

        const sockaddr* adr = session->get_transport()->address();
        int s = getnameinfo(reinterpret_cast<const sockaddr*>(adr),
            sizeof(struct sockaddr_in),
            buff, sizeof(buff), // socket_name, buffs,
            NULL, 0, NI_NUMERICHOST);
        if(s == 0)
            env.setenv("REMOTE_ADDRESS", buff);

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

//        printf("Before fork\n");

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

            char* argv[2];
            argv[1] = nullptr;
            argv[0] = executable;

//            printf("Before execve in child\n");
            execve(executable, argv, (char * const *) env.environ);
        }
        else				/* in the parent */
        {
            int status = 0;
            close(CHILD_READ);
            close(CHILD_WRITE);

//            printf("Before send form to child\n");

            /* do parent stuff */
            fp = fdopen(PARENT_READ, "rb");

            FILE* ofp = fdopen(PARENT_WRITE, "wb");
            if (!ofp) {
                perror("Unable fdopen write pipe. TODO: Check resoiurces leak here");
                continue;
            }
            int l = fwrite((const void*)request->body, 1, (size_t)request->_content_lenght, ofp);
            if (l != request->_content_lenght) {
                fprintf(stderr, "Sent broken response\n");
            }
            fclose(ofp);
            wait(&status);
//            printf("#### %d -> %d  STATUS: %d\n", request->_content_lenght, l, status);
        }

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