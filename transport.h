#pragma once
#include <arpa/inet.h>

typedef class transport_i {
public:
    virtual const sockaddr* const address() = 0;
    virtual int bind_and_listen(int port) = 0;
    virtual transport_i*  accept() = 0;
    virtual int handshake() = 0;
    virtual int recv(char * data, int size) = 0;
    virtual int send(char * data, int len) = 0;
    virtual int close() = 0;
    virtual int describe(char* socket_name, int buffs) = 0;
    virtual int is_secured() = 0;
    virtual ~transport_i() {}
} transport_t;

