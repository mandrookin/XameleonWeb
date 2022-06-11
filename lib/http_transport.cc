#include "../transport.h"

#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef class http_transport : public transport_i
{
    int socket;
    struct sockaddr_in addr;
    bool session_found;

protected:
    const sockaddr* const address();
    int describe(char* socket_name, int buffs);
    int bind_and_listen(int port);
    transport_i * accept();
    int handshake();
    int recv(char* data, int size);
    int send(char* data, int len);
    int close();
    ~http_transport();
public:
    http_transport();
} https_transport_t;

http_transport::http_transport()
{
//    context = c;
    session_found = false;
    printf("\033[36mHTTP transport constructor\033[0m\n");
}

http_transport::~http_transport() 
{
    printf("\033[36mHTTP transport destructor\033[0m\n");
}

const sockaddr* const http_transport::address()
{
    return reinterpret_cast<sockaddr*>(&addr);
}


#include <netdb.h>

int http_transport::describe(char* socket_name, int buffs)
{
    char name[64];
    if (buffs > 0)
        socket_name[0] = '\0';
    int s = getnameinfo(reinterpret_cast<sockaddr*>(&addr),
        sizeof(struct sockaddr_in),
        name, sizeof(name), // socket_name, buffs,
        NULL, 0, NI_NUMERICHOST);
    if (s != 0) {
        printf("http_transport::describe - getnameinfo() failed: %s\n", gai_strerror(s));
        return -1;
    }
    snprintf(socket_name, buffs, "http <- %s:%u", name, htons(addr.sin_port));
    return s;
}

int http_transport::bind_and_listen(int port)
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

int http_transport::close()
{
    return ::close(socket);
}

int http_transport::handshake()
{
//    printf("HTTP ready\n");
    return 0;
}

transport_i* http_transport::accept()
{
    unsigned int len = sizeof(addr);
    http_transport* peer = new http_transport();
    peer->socket = ::accept(socket, (struct sockaddr*)&peer->addr, &len);
    if (peer->socket < 0) {
        perror("http_transport::accept(): Unable accept incoming connection on TCP level");
        delete peer;
        peer = nullptr;
    }
    return peer;
}

int http_transport::recv(char* data, int size)
{
    return ::recv(socket, data, size, 0);
}

int http_transport::send(char* snd_buff, int len)
{
    return ::send(socket, snd_buff, len, 0);
}

transport_t* create_http_transport()
{
    return new http_transport();
}
