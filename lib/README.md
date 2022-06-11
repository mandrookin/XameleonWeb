# Библиотечные функции

## Сетевые транспорты

- реализация [TCP/IP транспорт](http_transport.cc)а для HTTP соединения  
- реализвйия [TCP/IP через SSL транспорт](https_transport.cc)а для HTTPS соединения

Возможна реализация транспорты поверх других протоколов. Для этого реализация должна быть наследована от абстактного класса ```transport_i```

```c++
typedef class transport_i {
public:
    virtual int describe(char * socket_name, int buffs) = 0;
    virtual int bind_and_listen(int port) = 0;
    virtual transport_i*  accept() = 0;
    virtual int handshake() = 0;
    virtual int recv(char * data, int size) = 0;
    virtual int send(char * data, int len) = 0;
    virtual int close() = 0;
    virtual ~transport_i() {}
} transport_t;
```

