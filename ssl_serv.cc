#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
//#include <fcntl.h>
#include <signal.h>

#include "transport.h"
#include "session.h"
#include "session_mgr.h"
#include "action.h"

#define HTTP_ONLY       false
#define SERVER_PORT     4433

static volatile bool        keepRunning = true;
static transport_i      *   transport;
static session_mgr_t        session_cache;
static const char       *   webpages = "pages/";
static int                  server_port = SERVER_PORT;

#if HTTP_ONLY
extern transport_t* create_http_transport();
#else
extern SSL_CTX* create_context();
extern transport_t* create_https_transport(SSL_CTX* ctx);
#endif

session_mgr_t* get_active_sessions()
{
    return &session_cache;
}

void *thread_func(void *data)
{
    char client_name[64];
    https_session_t* client = (https_session_t*)data;
    client->get_transport()->describe(client_name, sizeof(client_name));
    printf("Client '%s' thread started\n", client_name);
    session_cache.add_session(client);
    pthread_detach(pthread_self());
    client->https_session();
    session_cache.remove_session(client);
    delete client;
    printf("Client '%s' thread exit\n", client_name);
    pthread_exit(NULL);
}

// Segmentation fail handler
void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    fprintf(stderr, "Caught segfault at address %p\n", si->si_addr);
    exit(1);
}

void intHandler(int dummy) {
    keepRunning = false;
    transport->close();
}

void set_segfault_handler()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);

    signal(SIGINT, intHandler);
}

#include <ifaddrs.h>

void show_registered_interfaces()
{
    printf("HTTP server listen for incoming connections on:\n");
    struct ifaddrs * ifl;
    if (0 == getifaddrs(&ifl) && ifl != nullptr) {
        do {
            int s;
            char host[NI_MAXHOST];
            if (ifl->ifa_addr->sa_family == AF_INET) {
                s = getnameinfo(ifl->ifa_addr,
                    sizeof(struct sockaddr_in),
                    host, NI_MAXHOST,
                    NULL, 0, NI_NUMERICHOST);
                if (s != 0) {
                    printf("getnameinfo() failed: %s\n", gai_strerror(s));
                    exit(EXIT_FAILURE);
                }
                printf("\033[32m%s\033[0m: \033[33mhttps://%s:%u\033[0m\n", ifl->ifa_name, host, server_port);
            }
            //else
            //    printf("%s: Family value %d\n", ifl->ifa_name, ifl->ifa_addr->sa_family);

            ifl = ifl->ifa_next;

        } while (ifl);
        freeifaddrs(ifl);
    }
    else
        fprintf(stderr, "No interfaces detected\n");
}

void read_environment_variables()
{
    char* value = getenv("WEBPAGES");
    if (value && strlen(value) > 0) {
        webpages = value;
    }
    fprintf(stderr, "WEBPAGES path set to %s\n", webpages);
    value = getenv("HTTPS_PORT");
    if (value && strlen(value) > 0) {
        int port = atoi(value);
        if (port > 0 && port < USHRT_MAX) {
            server_port = port;
            fprintf(stderr, "server TCP port set %d\n", port);
        }
    }
}

int main(int argc, char **argv)
{
    pthread_t       handle;

    set_segfault_handler();
    read_environment_variables();

#if HTTP_ONLY
    transport = create_http_transport();
#else
    SSL_CTX * context = create_context();
    transport = create_https_transport(context);
#endif
    transport->bind_and_listen(server_port);

    http_action_t* static_page = new static_page_action(guest);
    add_action_route("/", GET, static_page);
    add_action_route("/", POST, new post_form_action(tracked));
    add_action_route("/favicon.ico", GET, new get_favicon_action);
    add_action_route("/reload/", GET, new get_touch_action);
    add_action_route("/upload", GET, new get_directory_action(guest));
    add_action_route("/upload/", GET, static_page);

    http_action_t* cgi_bin = new cgi_action(guest);
    add_action_route("/cgi/", GET, cgi_bin);
    add_action_route("/cgi/", POST, cgi_bin);

    http_action_t* admin = new admin_action;
    add_action_route("/admin", GET, admin);
    add_action_route("/admin/", GET, admin);

    show_registered_interfaces();

    transport_i* client_transport = nullptr;

    while (keepRunning) 
    {
        client_transport = transport->accept();
        if ( !client_transport) {
            if (keepRunning)
                perror("Unable accept incoming connnection");
            else
                puts("\033[36m\nServer stopped by TERM signal\033[0m\n");
            continue;
        }
        https_session_t* client = new https_session_t(client_transport, webpages);
        if (pthread_create(&handle, NULL, thread_func, client)) {
            perror("Unable create client thread");
        }
    }

    if (transport) {
        transport->close();
        delete transport;
    }
#if ! HTTP_ONLY
    SSL_CTX_free(context);
#endif

    return 0;
}
