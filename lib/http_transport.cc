#define _CRT_SECURE_NO_WARNINGS 1
#include "../transport.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
//#define closesocket close
#else
#include <winsock.h>
#pragma comment(lib, "Ws2_32.lib")
#define socklen_t int
#endif

#include <stdio.h>
#include <string.h>

namespace xameleon
{

    typedef class http_transport : public transport_i
    {
        int socket;
        struct sockaddr_in addr;
        bool session_found;

    protected:
        const sockaddr* const address();
        int describe(char* socket_name, int buffs);
        int bind_and_listen(int port);
        transport_i* accept();
        int handshake();
        int recv(char* data, int size);
        int recv(char* data, int size, long long timeout);
        int send(char* data, int len);
        int close();
        int is_secured() { return 0; }
        int connect(const char* hostname, int port);
        ~http_transport();
    public:
        http_transport();
    } https_transport_t;

    http_transport::http_transport()
    {
        socket = -1;
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

#ifndef _WIN32

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
#else
    int http_transport::describe(char* socket_name, int buffs)
    {
        strncpy(socket_name, "not imlemented\n", buffs);
        return 0;
    }
#endif

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

#ifndef _WIN32
        // Forcefully attaching server_socket to the port 8080
        if (setsockopt(socket, SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT, &opt,
            sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
#endif

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
#ifndef _WIN32
        return ::close(socket);
#else
        return ::closesocket(socket);
#endif
    }

    int http_transport::handshake()
    {
        //    printf("HTTP ready\n");
        return 0;
    }

    transport_i* http_transport::accept()
    {
        socklen_t len = sizeof(addr);
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
        int status, bytes = 0;
        while (bytes < len)
        {
            status = ::send(socket, snd_buff, len, 0);
            if (status < 0)
                return status;
            bytes += status;
        }
        return bytes;
    }

    int http_transport::recv(char* data, int size, long long time)
    {
        fd_set set;
        struct timeval timeout;

        FD_ZERO(&set);
        FD_SET(socket, &set);

        timeout.tv_sec = time / 1000000;;
        timeout.tv_usec = time % 1000000;;

        /* select returns 0 if timeout, 1 if input available, -1 if error. */
        int ev = select(socket + 1, &set, NULL, NULL, &timeout);
        if (ev == 0) {
            printf("Socket timeout. Start closing procedure\n");
            return 0;
        }
        if (ev < 0) {
            fprintf(stderr, "Socket error on select with timeout: %d\n", errno);
            return ev;
        }

        return ::recv(socket, data, size, 0);
    }

    int http_transport::connect(const char* hostname, int port)
    {
        int status = -2;
        do {
            socket = -1;
            int addr_len = sizeof(addr);

            bool host_found = get_address_by_name(hostname, &addr, &addr_len);
            if (!host_found) {
                fprintf(stderr, "\nHost '%s' not found\n", hostname);
                status = -5;
                break;
            }

            if ((socket = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                fprintf(stderr, "creation of proxy socket failed\n");
                status = -4;
                break;
            }

            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);

            status = ::connect(socket, (struct sockaddr*)&addr, addr_len);
            if (status < 0) {
                close();
                socket = -1;
                fprintf(stderr, "\nConnection Failed \n");
                break;
            }
        } while (false);
        return status;
    }

    transport_t* create_http_transport()
    {
        return new http_transport();
    }

}

