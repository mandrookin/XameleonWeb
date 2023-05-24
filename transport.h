#pragma once


#ifndef _WIN32
#include <arpa/inet.h>
#define SOCKET int
#else

//typedef struct { char  payload[256]; } sockaddr;
//#include <WinSock2.h>
#include <winsock.h>
#endif

namespace xameleon {
    class transport_i {
    public:
        virtual const sockaddr* const address() = 0;
        virtual int bind_and_listen(int port) = 0;
        virtual transport_i* accept() = 0;
        virtual int handshake() = 0;
        virtual int recv(char* data, int size) = 0;
        virtual int recv(char* data, int size, long long timeout) = 0;
        virtual int send(const char* data, int len) = 0;
        virtual int close() = 0;
        virtual int describe(char* socket_name, int buffs) = 0;
        virtual int is_secured() = 0;
        virtual int connect(const char* hostname, int port) = 0;
        virtual ~transport_i() {}
    };

    bool get_address_by_name(const char* dns_name, struct sockaddr_in* paddress, int* addr_len);
    transport_i* get_http_transport(const char * name);
    void release_http_transport(transport_i* tp);
}

