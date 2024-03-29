﻿# Библиотечные функции

## Сетевые транспорты

- реализация [TCP/IP транспорт](http_transport.cc)а для HTTP соединения  
- реализвйия [TCP/IP через SSL транспорт](https_transport.cc)а для HTTPS соединения

Возможна реализация транспортов поверх других протоколов, например поверх UDP. Для этого реализация транспортного должна быть наследована от абстактного класса ```transport_i```

```c++
typedef class transport_i {
public:
    virtual const sockaddr* const address() = 0;
    virtual int bind_and_listen(int port) = 0;
    virtual transport_i*  accept() = 0;
    virtual int handshake() = 0;
    virtual int recv(char * data, int size) = 0;
    virtual int recv(char* data, int size, long long timeout) = 0;
    virtual int send(char * data, int len) = 0;
    virtual int close() = 0;
    virtual int describe(char* socket_name, int buffs) = 0;
    virtual int is_secured() = 0;
    virtual int connect(const char * hostname, int port) = 0;
    virtual ~transport_i() {}
} transport_t;

```

