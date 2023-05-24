#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include "../transport.h"

namespace xameleon
{

    class FileTransport : public transport_i {
    public:
        FILE* file_descritor;
        FILE* devnull;
    private:
        const sockaddr* const address() { return (sockaddr*)file_descritor; };
        int bind_and_listen(int port) { throw "bind_and_listen"; }
        transport_i* accept() { throw "accept"; }
        int handshake() {
            fprintf(devnull, "handshake FileTransport\n");
            return 0;
        }
        int recv(char* data, int size) {
            return (int)fread(data, 1, size, file_descritor);
            throw "recv";
        }
        int recv(char* data, int size, long long timeout)
        {
            return recv(data, size);
        }
        int send(const char* data, int len)
        {
            fprintf(devnull, data);
            return len;
        }
        int close()
        {
            if(file_descritor != nullptr){
                fprintf(stderr, "close file\n");
                fclose(file_descritor);
            }
            return 0;
        }
        int describe(char* socket_name, int buffs)
        {
            return snprintf(socket_name, buffs, "file:");
        }
        int is_secured() { return true; }
        int connect(const char* hostname, int port) { return -1; }
    public:
        virtual ~FileTransport() {
            if (devnull)
                fclose(devnull);
        }
        FileTransport(FILE* fd) {
            this->file_descritor = fd;
            devnull = fopen("nul", "w");
        }
    };

transport_i* create_fiel_name_transport(char * filename)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        perror("Unable open multipart file");
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    transport_i* transport = new FileTransport(fp);
    return transport;
}

}

xameleon::transport_i* create_file_descriptor_transport(FILE* fp)
{
    return new xameleon::FileTransport(fp);
}

