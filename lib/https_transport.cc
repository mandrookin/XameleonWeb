#include "../transport.h"

#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef class https_transport : public transport_i
{
    int socket;
    struct sockaddr_in addr;
    SSL_CTX* context;
    SSL* ssl;
    bool session_found;

protected:
    const sockaddr* const address();
    int bind_and_listen(int port);
    transport_i * accept();
    int handshake();
    int recv(char* data, int size);
    int send(char* data, int len);
    int close();
    int describe(char* socket_name, int buffs);
    int is_secured() { return 1; };
    ~https_transport();
public:
    https_transport(SSL_CTX* c);
} https_transport_t;

https_transport::https_transport(SSL_CTX* c)
{
    context = c;
    session_found = false;
    printf("\033[36mHTTPS transport constructor\033[0m\n");
}

https_transport::~https_transport() 
{
    printf("\033[36mHTTPS transport destructor\033[0m\n");
    if (ssl != nullptr) {
//        SSL_shutdown(ssl);
//        SSL_free(ssl);
    }
}

#include <netdb.h>

const sockaddr* const https_transport::address()
{
    return reinterpret_cast<sockaddr*>(&addr);
}

int https_transport::describe(char* socket_name, int buffs)
{
    char name[64];
    if (buffs > 0)
        socket_name[0] = '\0';
//   int s = getnameinfo((const sockaddr*)(&addr),
    int s = getnameinfo(reinterpret_cast<sockaddr*>(&addr),
        sizeof(struct sockaddr_in),
        name, sizeof(name), // socket_name, buffs,
        NULL, 0, NI_NUMERICHOST);
    if (s != 0) {
        printf("http_transport::describe - getnameinfo() failed: %s\n", gai_strerror(s));
        return -1;
    }
    snprintf(socket_name, buffs, "HTTPS <- %s:%u", name, htons(addr.sin_port));
    return s;
}

int https_transport::bind_and_listen(int port)
{
    int opt = 1;
      
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0) {
        perror("Unable create server_socket");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching server_socket to the port 8080
    if (setsockopt(socket, SOL_SOCKET,
        SO_REUSEADDR | SO_REUSEPORT, &opt,
        sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable bind server_socket");
        exit(EXIT_FAILURE);
    }

    if (listen(socket, 1) < 0) {
        perror("Unable listen server_socket");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int https_transport::close()
{
//    SSL_shutdown(ssl);
//    SSL_free(ssl);
    return ::close(socket);
}

int https_transport::handshake()
{
    ssl = SSL_new(context);
//    printf("Socket %d Context %p ssl %p\n", socket, context, ssl);
    SSL_set_fd(ssl, socket);

    //    SSL_set_msg_callback(ssl, SSL_callback);

    int ssl_status = SSL_accept(ssl);
    if (ssl_status < 0) {
        perror("Unable accept incoming SSL connection");
        ::close(socket);
        return -1;
    }
    if (ssl_status == 0) {
        fprintf(stderr, "Self signed certificate?\n");
    }

    X509* peer_cert = SSL_get_peer_certificate(ssl);
    if (peer_cert == nullptr) {
        printf("No peer certificate detected\n");
    }

    //        SSL_SESSION_set1_id_context(session, (const unsigned char*) "WHAT IS THAT?", 14);

    SSL_SESSION* session = SSL_get_session(ssl);
    printf("SSL_SESSION_is_reusable %d\n", SSL_SESSION_is_resumable(session));

    unsigned int context_len;
    const unsigned char* context_buff;

    context_buff = SSL_SESSION_get0_id_context(session, &context_len);
    if (context_len != 0) {
        if (strcmp((const char*)context_buff, "Pressure") != 0) {
            printf("Session context: %s\n", context_buff);
            session_found = true;
        }
    }

    return session_found;
}

transport_i* https_transport::accept()
{
    unsigned int len = sizeof(addr);
    https_transport* peer = new https_transport(context);
    peer->socket = ::accept(socket, (struct sockaddr*)&peer->addr, &len);
    if (peer->socket < 0) {
        perror("https_transport::accept(): Unable accept incoming connection on TCP level");
        delete peer;
        peer = nullptr;
    }
    return peer;
}

int https_transport::recv(char* data, int size)
{
    int status = SSL_read(ssl, data, size);
//    printf("recv %u byes:\n%s\n", status, data);
    return status;
}

int https_transport::send(char* snd_buff, int len)
{
    int status = SSL_write(ssl, snd_buff, len);
//    printf("sent %u byes:\n%s\n", status, snd_buff);
    return status;
}

transport_t* create_https_transport(SSL_CTX* ctx)
{
    return new https_transport(ctx);
}
