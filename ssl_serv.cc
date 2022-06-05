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

#include "session.h"
#include "action.h"

#define SERVER_PORT     4433

static volatile bool keepRunning = true;
static volatile int server_sock;

int create_socket(int port)
{
    int s;
    int opt = 1;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(s, SOL_SOCKET,
        SO_REUSEADDR | SO_REUSEPORT, &opt,
        sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

extern SSL_CTX *create_context();

void *thread_func(void *data)
{
    printf("Client thread started\n");
    https_session_t * client = (https_session_t*)data;
    pthread_detach(pthread_self());
    client->https_session();
    delete client;
    printf("Client thread exit\n");
    pthread_exit(NULL);
}

// Segmentation fail handler
void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    fprintf(stderr, "Caught segfault at address %p\n", si->si_addr);
    exit(1);
}

void intHandler(int dummy) {
    close(server_sock);
    keepRunning = false;
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
                printf("\033[32m%s\033[0m: \033[33mhttps://%s:%u\033[0m\n", ifl->ifa_name, host, SERVER_PORT );
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

int main(int argc, char **argv)
{
    int client_sock;
    pthread_t handle;

    set_segfault_handler();

    SSL_CTX * context = create_context();

    server_sock = create_socket(SERVER_PORT);

    add_action_route("/", GET, new static_page_action(tracked));
    add_action_route("/", POST, new post_form_action(tracked));
    add_action_route("/favicon.ico", GET, new get_favicon_action);
    add_action_route("/reload/", GET, new get_touch_action);
    add_action_route("/dir", GET, new get_directory_action(guest));
    add_action_route("/dir/", GET, new static_page_action(guest));
    add_action_route("/cgi/", GET, new get_cgi_action(guest));
    add_action_route("/cgi/", POST, new get_cgi_action(guest));

    show_registered_interfaces();

    while (keepRunning) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        client_sock = accept(server_sock, (struct sockaddr*)&addr, &len);
        if (client_sock < 0) {
            if (keepRunning)
                perror("Unable to accept incoming connnection");
            else
                puts("\033[36m\nServer stopped by TERM signal\033[0m\n");
            continue;
        }
        https_session_t	* client = new https_session_t(client_sock, server_sock, context);
        if (pthread_create(&handle, NULL, thread_func, client)) {
            perror("Unable create client thread");
        }
    }
    close(server_sock);
    SSL_CTX_free(context);
    return 0;
}
