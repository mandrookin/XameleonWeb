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
#include <list>

namespace xameleon
{

    typedef class http_transport : public transport_i
    {
        SOCKET socket;
        struct sockaddr_in addr;
        bool session_found;
        char name[50];

    protected:
        const sockaddr* const address();
        int describe(char* socket_name, int buffs);
        int bind_and_listen(int port);
        transport_i* accept();
        int handshake();
        int recv(char* data, int size);
        int recv(char* data, int size, long long timeout);
        int send(const char* snd_buff, int len);
        int close();
        int is_secured() { return 0; }
        int connect(const char* hostname, int port);
        ~http_transport();
    public:
        http_transport(const char* name);
    } https_transport_t;

    http_transport::http_transport(const char* name)
    {
        strncpy(this->name, name, sizeof(this->name));
        socket = -1;
        session_found = false;
        printf("\033[36mHTTP transport constructor\033[0m: %s\n", name);
    }

    http_transport::~http_transport()
    {
        printf("\033[36mHTTP transport destructor\033[0m: %s\n", name);
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
        int status;
        sockaddr_in     sa;
        int sa_len = sizeof(sa);

        status = getpeername(socket, (sockaddr*)&sa, &sa_len);
        snprintf(socket_name, buffs, "%u.%u.%u.%u:%u",
            sa.sin_addr.S_un.S_un_b.s_b1,
            sa.sin_addr.S_un.S_un_b.s_b2,
            sa.sin_addr.S_un.S_un_b.s_b3,
            sa.sin_addr.S_un.S_un_b.s_b4,
            sa.sin_port);
        return status;
    }
#endif

    int http_transport::bind_and_listen(int port)
    {
        int opt = 1;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        socket = (int) ::socket(AF_INET, SOCK_STREAM, 0);
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
        if (socket < 0)
            return (int) socket;
#ifndef _WIN32
        return ::close(socket);
#else
            int rval = ::closesocket(socket);
            socket = -1;
            return rval;
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
        http_transport* peer = nullptr;
        struct sockaddr_in addr;
        SOCKET sock = ::accept(socket, (struct sockaddr*)&addr, &len);
        if (sock < 0) {
            perror("http_transport::accept(): Unable accept incoming connection on TCP level");
            return nullptr;
        }

        char name[80];
        snprintf(name, sizeof(name), "%u.%u.%u.%u:%u",
            addr.sin_addr.S_un.S_un_b.s_b1,
            addr.sin_addr.S_un.S_un_b.s_b2,
            addr.sin_addr.S_un.S_un_b.s_b3,
            addr.sin_addr.S_un.S_un_b.s_b4,
            addr.sin_port);
        peer = new http_transport(name);
        memcpy(&peer->addr, &addr, sizeof(struct sockaddr_in));
        peer->socket = sock;
        return peer;
    }

    int http_transport::recv(char* data, int size)
    {
        return ::recv(socket, data, size, 0);
    }

    int http_transport::send(const char* snd_buff, int len)
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

        if (socket < 0)
            return -1;

        FD_ZERO(&set);
        FD_SET(socket, &set);

        timeout.tv_sec = (long)(time / 1000000);
        timeout.tv_usec = time % 1000000;;

        /* select returns 0 if timeout, 1 if input available, -1 if error. */
        int ev = select((int)socket + 1, &set, NULL, NULL, &timeout);
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
                int error_no = WSAGetLastError();
                close();
                socket = -1;
                fprintf(stderr, "\nConnection Failed: %d\n", error_no);
                break;
            }
        } while (false);
        return status;
    }

    static std::list<transport_i*>        free_transports;

    transport_i* get_http_transport(const char* name)
    {
        transport_i* result = nullptr;
        if (free_transports.size() > 0) {
            result = free_transports.front();
            free_transports.pop_front();
        }
        else {
            result = new http_transport(name);
        }
        return result;
    }

    void release_http_transport(transport_i* tp)
    {
        free_transports.push_back(tp);
    }
}

