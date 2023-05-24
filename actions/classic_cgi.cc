#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif // _WIN32


#include <sys/stat.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <netdb.h>
#else
#endif

#include "../action.h"
#include "../lib.h"
#include "../stream_writer.h"

using namespace xameleon;


typedef struct env {
    int count;
    int size;
    const char* environment[32];
    char holder[4096];
    env() { for (int i = 0; i < 32; i++) environment[i] = nullptr; count = 0; size = 0; }
    int setenv(const char* name, const char* value);
} env_t;

int env_t::setenv(const char* name, const char* value)
{
    if (count > 31 || (sizeof(holder) - size) < 5) {
        fprintf(stderr, "CGI environment overflow\n");
        return -1;
    }

    int len = snprintf(holder + size, sizeof(holder) - size, "%s=%s", name, value);

    environment[count] = holder + size;

    size += len + 1;
    count++;
    return count;
}

#ifdef _WIN32

#define MAXRESPONSE (64*1024)

void cgi_action::process_req(https_session_t* session)
{
    url_t* url = &session->request.url;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    CHAR process_name[512];

    SECURITY_ATTRIBUTES saAttr;

    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    // Create a pipe for the child process's STDOUT. 
    HANDLE m_hChildStd_OUT_Rd, m_hChildStd_OUT_Wr;
    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0))
    {
        // log error
        //return HRESULT_FROM_WIN32(GetLastError());
        throw "Classic CGI: unable create pipe";
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.

    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
    {
        // log error
        //return HRESULT_FROM_WIN32(GetLastError());
        throw "Classic CGI: something goes wrong";
    }
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = m_hChildStd_OUT_Wr;
    si.hStdOutput = m_hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

// TODO:    _dupenv_s()
    snprintf(process_name, sizeof(process_name), "%s /c dir", getenv("COMSPEC"));

    // Start the child process. 
    if (!CreateProcess(NULL,   // No module name (use command line)
        process_name,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        TRUE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return;
    }
    else 
    {
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process, for example. 

//        CloseHandle(pi.hProcess);
//        CloseHandle(pi.hThread);

        // Close handles to the stdin and stdout pipes no longer needed by the child process.
        // If they are not explicitly closed, there is no way to recognize that the child process has ended.

        CloseHandle(si.hStdOutput); //  g_hChildStd_OUT_Wr);
        CloseHandle(si.hStdInput); // g_hChildStd_IN_Rd);
    }

    stream_writer   sender(session);
    session->response_holder._body = new char[MAXRESPONSE];
    session->response_holder._body_size = 0;

    for (;;)
    {
        DWORD dwRead;
        CHAR chBuf[2048];
        BOOL bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, sizeof(chBuf)-1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) {
            break;
        }
        chBuf[dwRead] = 0;
        chBuf[sizeof(chBuf) - 1] = 0;

        if (session->response_holder._body_size + dwRead > MAXRESPONSE)
            break;
        memcpy( (void*)(session->response_holder._body + session->response_holder._body_size), chBuf, dwRead);
        session->response_holder._body_size += dwRead;

        fprintf(stdout, chBuf);
    }
    fprintf(stdout, "\n");

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
//    session->page_not_found(GET, url->path, session->request._referer.c_str());

    session->response_holder._content_type = "text/plain; charset=utf-8";
    session->response_holder._header_size = session->response_holder.prepare_header(
        session->response_holder._header, 
        session->response_holder._code, 
        session->response_holder._body_size);
    session->send_prepared_response();
    delete session->response_holder._body;

#if false

while (fgets(buff, BUFSIZE, fp) != NULL) {
    int l = strlen(buff);
    sender.send(false, buff, l);
}

sender.send(true, nullptr, 0);
sender.close_on_end();

if (pclose(fp)) {
    printf("Command not found or exited with error status\n");
    session->page_not_found(GET, url->path, request->_referer.c_str());
    continue;
}
#endif
}




#else

void cgi_action::process_req(https_session_t* session)
{
    url_t* url = &session->request.url;
    http_request_t* request = &session->request;
    http_response_t* response = &session->response_holder;
    env_t                   env;
    int status;
    struct stat path_stat;
    const int BUFSIZE = 512;
    const int MAXRESPONSE = 64 * 1024;
    FILE* fp = nullptr;
    char  executable[BUFSIZE];
    char  buff[BUFSIZE];

    printf("ACTION: CGI [%s%s <- %s]\nHeader size = %d url->query_count = %d\n",
        session->get_html_root(), url->path, url->rest, response->_header_size, url->query_count);


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


        env.setenv("REQUEST_METHOD", http_method(url->method));
        env.setenv("SCRIPT_NAME", url->path);
        env.setenv("CONTENT_LENGTH", std::to_string(request->_content_lenght).c_str());
        std::string path = getenv("PATH");
        env.setenv("PATH", path.c_str());

        const sockaddr * adr = session->get_transport()->address();
        int s = getnameinfo(reinterpret_cast<const sockaddr*>(adr),
            sizeof(struct sockaddr_in),
            buff, sizeof(buff), // socket_name, buffs,
            NULL, 0, NI_NUMERICHOST);
        if (s == 0)
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
            execve(executable, argv, (char* const*)env.environment);
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

            //  printf("Request body %p content lenght %d\n", request->body, request->_content_lenght);
            const char* p;
            char* o = buff;
            int fb = sizeof(buff);
            int len = request->_content_lenght;
            int count = 0;
            bool sync;
            int tmp_size;
            multipart_stream_reader_t* reader = session->get_multiart_reader();
            while (len > 0) {
                p = reader->get_pchar(sync, tmp_size);
                if (sync) {
                    printf("CGI multipart not supported yet\n");
                    break;
                }
                *o++ = *p;
                fb--;
                len--;
                count++;
                if (fb == 0 || len == 0) {
                    //buff[count] = 0;
                    //printf("%6d: '%s'\n", count, buff);
                    int l = fwrite((const void*)buff, 1, (size_t)count, ofp);
                    if (l != count) {
                        fprintf(stderr, "Unable sent http to classic CGI script\n");
                    }
                }
            }

            fclose(ofp);
            wait(&status);
            //            printf("#### %d -> %d  STATUS: %d\n", request->_content_lenght, l, status);
        }

#if true
        response->_body = new char[MAXRESPONSE];
        response->_body_size = 0;

        while (fgets(buff, BUFSIZE, fp) != NULL) {
            int l = strlen(buff);
            //printf("OUTPUT: %s", buff);
            if (response->_body_size + l > MAXRESPONSE)
                break;
            memcpy(response->_body + response->_body_size, buff, l);
            response->_body_size += l;
        }
        response->_content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);
        session->send_prepared_response();
        delete response->_body;
#else

        response->_content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, response->_code, 0);
        stream_writer   sender(session);

        while (fgets(buff, BUFSIZE, fp) != NULL) {
            int l = strlen(buff);
            sender.send(false, buff, l);
        }

        sender.send(true, nullptr, 0);
        sender.close_on_end();
#endif

        if (pclose(fp)) {
            printf("Command not found or exited with error status\n");
            session->page_not_found(GET, url->path, request->_referer.c_str());
            continue;
        }

    } while (false);
}

#endif