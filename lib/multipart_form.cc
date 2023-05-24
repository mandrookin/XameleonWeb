/*

*/
#define _CRT_SECURE_NO_WARNINGS 1
#define DEBUG_FORM
#define DEBUG_MSVS

// Если определить, компилятор ругается на опасную устаревшую функцию
#define TMPNAM 1

// 中国

#include <cstring>
//#include <ios>
#include <fstream>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/fcntl.h>
#define _unlink unlink
#else
#include "../transport.h"
#include "multipart_stream_reader.h"
#endif

#include "../session.h"
#include "../multipart_form.h"

unsigned long get_hash_value(const char* w);

namespace xameleon
{

#if false

    static const int B64index[256] =
    {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
        7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
        0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };

    class base64_decoder : public content_decoder_i
    {
    private:
        int step_in;
        int step_out;
        int n;
        char stream(const char in, state_t& need_more);
        char stream(state_t& need_more);
    public:
        base64_decoder() { step_in = step_out = n = 0; }
        ~base64_decoder() {}
    };

    char base64_decoder::stream(const char in, state_t& need_more)
    {
        if (in == '\r' || in == '\n')
        {
            status = need_more;
            return ch;
        }
        unsigned char ch = '\0';
        need_more = base64_decoder::need_more;
        switch (step_in)
        {
        case 0:
            n = B64index[in] << 18;
            break;
        case 1:
            n |= B64index[in] << 12;
            break;
        case 2:
            n |= B64index[in] << 6;
            break;
        case 3:
            n |= B64index[in];
            need_more = base64_decoder::have_more;
            ch = n >> 16;
            step_out = 1;
            break;
        }
        step_in = (step_in + 1) % 4;
        return (char)ch;
    }

    char base64_decoder::stream(state_t& need_more)
    {
        unsigned char ch = '\0';
        switch (step_out)
        {
        case 1:
            step_out = 2;
            need_more = base64_decoder::have_more;
            ch = n >> 8 & 0xFF;
            break;
        case 2:
            step_out = 0;
            need_more = base64_decoder::data;
            ch = n & 0xFF;
            break;
        default:
            throw "Unarsed stated of base64_decoder::stream(state_t& need_more)";
        }
        return (char)ch;
    }

#else

    static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                    'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                    '4', '5', '6', '7', '8', '9', '+', '/' };
    static char* decoding_table = NULL;
//    static int mod_table[] = { 0, 2, 1 };

    class base64_decoder : public content_decoder_i
    {
        typedef enum { one, two, three, four, five, six, last } stage_t;
        stage_t stage;
        uint32_t sextet_a;
        uint32_t sextet_b;
        uint32_t sextet_c;
        uint32_t sextet_d;
        uint32_t triple;
        int skiper;
    private:
        char stream(const char in, state_t& need_more);
        char stream(state_t& need_more);
    public:
        base64_decoder() {
            if (decoding_table == nullptr)
            {
                decoding_table = new char[256];

                for (int i = 0; i < 64; i++)
                    decoding_table[(unsigned char)encoding_table[i]] = i;
            }
            stage = one;
            skiper = 0;
        }
        ~base64_decoder() {
        }
    };

    char base64_decoder::stream(const char in, state_t& status)
    {
        unsigned char ch = 0;
        if (in == '\r' || in == '\n')
        {
            status = need_more;
            return ch;
        }
        switch (stage)
        {
        case one:
            if (in == '=') {
                sextet_a = 0;
                skiper++;
            }
            else
                sextet_a = decoding_table[(int)in];
            stage = two;
            status = need_more;
            break;
        case two:
            if (in == '=') {
                sextet_b = 0;
                skiper++;
            }
            else
                sextet_b = decoding_table[(int)in];
            stage = three;
            status = need_more;
            break;
        case three:
            if (in == '=') {
                sextet_c = 0;
                skiper++;
            }
            else
                sextet_c = decoding_table[(int)in];
            stage = four;
            status = need_more;
            break;
        case four:
            if (in == '=') {
                sextet_d = 0;
                skiper++;
            }
            else
                sextet_d = decoding_table[(int)in];

            switch (skiper)
            {
            case 0:
                stage = five;
                status = have_more;
                break;
            case 1:
                stage = last;
                status = have_more;
                break;
            case 2:
                status = data;
                stage = one;
                break;
            default:
                printf("skiper = %d\n", skiper);
                break;
            }

            triple = (sextet_a << 3 * 6)
                + (sextet_b << 2 * 6)
                + (sextet_c << 1 * 6)
                + (sextet_d << 0 * 6);

            ch = (triple >> 2 * 8) & 0xFF;
            break;

        default:
            throw "use base64_decoder::stream(state_t& need_more)";

        }
        return ch;
    }

    char base64_decoder::stream(state_t& status)
    {
        unsigned char ch;
        switch (stage)
        {
        case five:
            ch = (triple >> 1 * 8) & 0xFF;
            stage = six;
            status = have_more;
            break;
        case six:
            ch = (triple >> 0 * 8) & 0xFF;
            stage = one;
            status = data;
            break;
        case last:
            ch = (triple >> 1 * 8) & 0xFF;
            stage = one;
            status = data;
            break;
        default:
            throw "use base64_decoder::stream(const char in, state_t& need_more)";
        }
        return (char)ch;
    }

#endif

    class quouted_printable_decoder : public content_decoder_i
    {
        typedef enum { passthrough, first, second, crlf } states_t;
        states_t    state;
        char        ch;
        char stream(const char in, state_t& need_more);
        char stream(state_t& need_more);
        static char toXDigit(char c)
        {
            if (c >= 0x41) {
                if (c >= 0x61)
                    return (char)(c - (0x61 - 0x0a));

                return (char)(c - (0x41 - 0x0A));
            }

            return (char)(c - 0x30);
        }

    public:
        quouted_printable_decoder() {
            state = passthrough;
            ch = 0;
        }
        ~quouted_printable_decoder() {}
    };

    char quouted_printable_decoder::stream(const char in, state_t& status)
    {
        switch (state)
        {
        case passthrough:
            if (in != '=') {
                ch = in;
                status = data;
            }
            else {
                state = first;
                status = need_more;
            }
            break;
        case first:
            if (in == '\r') {
                ch = in;
                state = crlf;
                status = need_more;
            }
            else if (isxdigit(in)) {
                ch = toXDigit(in) << 4;
                status = need_more;
                state = second;
            }
            else {
                status = data;
                ch = in;
            }
            break;

        case  second:
            if (isxdigit(in)) {
                ch |= toXDigit(in);
                status = data;
                state = passthrough;
            }
            else {
            }
            break;

        case crlf:
            if (in == '\n') {
                ch = 0;
                status = need_more;
                state = passthrough;
            }
            else {
                ch = in;
                status = have_more;
                return '\r';
            }
            break;

        default:
            break;
        }
        return ch;
    }

    char quouted_printable_decoder::stream(state_t& status)
    {
        if (state == crlf) {
            state = passthrough;
            status = data;
        }
        else
            throw "quouted_printable_decode FSM error";
        return ch;
    }


    region::~region()
    {
        destroy_object();
        //    fprintf(stdout, "Delete section %s\n", name.c_str());
    }

    form_data_t::form(const char* name)
    {
        _name = name;
        fprintf(stdout, "Create form %s\n", _name.c_str());
    }

    form_data_t::~form()
    {
        fprintf(stdout, "Destroy form '%s'\n", _name.c_str());
        for (auto object : this->_objects) {
            delete object;
        }
        this->_objects.clear();
    }

    int form_data_t::close_temp_files()
    {
        for (auto& section : _objects)
            section->close_object();
        return 0;
    }


    //static const int CACHE_SIZE = 16 * 1024;

    static int get_file_name(FILE* fp, char* namebuf, int bufsize)
    {
#ifdef _WIN32
        int nr = 0;
#else
        ssize_t nr;
#ifndef __linux__
        int fd = fileno(fp);
        // Тут может навернуться, если маленький буфер передать
        if (fcntl(fd, F_GETPATH, namebuf) == -1)
        {
            perror("Unable get name of temporary file");
            return -1;
        }
        nr = strlen(namebuf);
#else
        char fnmbuf[128];
        snprintf(fnmbuf, sizeof(fnmbuf), "/proc/self/fd/%d", fileno(fp));
        nr = readlink(fnmbuf, namebuf, bufsize);
        if (nr < 0)
            return -1;
        else namebuf[nr] = '\0';
#endif
#endif
        return nr;
    }


    typedef enum
    {
        UnknownContent,
        Delimiter,
        ContentType = hash("content-type"),
        ContentID = hash("content-id"),
        ContentTransferEncoding = hash("content-transfer-encoding"),
        ContentLocation = hash("content-Location"),
        ContentDisposition = hash("content-disposition"),
    } content_type_t;

    typedef enum {
        FormData = hash("form-data"), // 0x9e61dc8f,
        Attachment,
        Inline,
        // --
        Name = hash("name"), //0x53191060,
        FileName = hash("filename") // 0x239bf327
    } position_type_t;


    static char* read_property(char* memstream, int size, char* value_buffer, int valbuf_size, content_type_t& prop_type)
    {
        char* tmp_dbg = value_buffer;
        char* delimiter = memstream;
        bool value = false;
        for (int i = 0; i < size; i++)
        {
            switch (*delimiter)
            {
            case ':':
                if (!value) {
                    *delimiter = 0;
                    prop_type = (content_type_t)hash(memstream);
                    *delimiter = ':';
                    value = true;
                    delimiter++;
                    if (*delimiter != ' ')
                        throw "Delimiter absent";
                    delimiter++;
                    continue;
                }
                break;
            case ';':
                break;
            case 0xd:
                i = size - 2;
                if (value)
                {
                    value = false;
                    *value_buffer = 0;
                }
                else
                {
                    prop_type = content_type_t::Delimiter;
                }
                break;
            case 0xa:
                i = size;
                return ++delimiter;
            }
            if (value)
            {
                if (valbuf_size > 0) {
                    *value_buffer++ = *delimiter;
                    valbuf_size--;
                }
            }
            delimiter++;
        }

        return delimiter;
    }

    const char* get_property_name(content_type_t type)
    {
        switch (type)
        {
        case ContentType:
            return "ContentType";
        case ContentID:
            return "ContentID";
        case ContentTransferEncoding:
            return "ContentTransferEncoding";
        case ContentLocation:
            return "ContentLocation";
        case ContentDisposition:
            return "ContentDisposition";
        default:
            return "Unknown content-type";
        }
    }

    int section_t::create_object()
    {
        if (this->content_type.empty())
            type = region::text;
        else {
            contect_type_t ctype = (contect_type_t)hash(this->content_type.c_str());
            switch (ctype)
            {
            case text_css:
                type = region::text;
                break;
            case octet_stream:
            case image_gif:
            case text_xml:
            {
                char namebuf[512];
                if (!filename.empty())
                {
                    char* nowarn = tmpnam(namebuf);
                    printf("%s\n", nowarn);
                    this->filename = namebuf;
                    type = region::file;
                    this->fp = fopen(this->filename.c_str(), "wb");
                }
                else
                    printf("Skip empty section\n");
                break;
            }
            case text_html:
            case text_plain:
            case image_png:
            case image_jpeg:
                type = region::file;
                // Поскольку файл не открыт - игнор этих вложений.
                break;
            default:
                printf("\033[33mUnknown multipart content-type\033[0m: %s\n", content_type.c_str());
                state = sync;
                type = sync;
                return 0;
            }

            if (!this->transfer_encoding.empty())
            {
                encoding = (transfer_encoding_t)hash(this->transfer_encoding.c_str());
                switch (encoding)
                {
                case hash("quoted_printable"):
                    this->decoder = new quouted_printable_decoder;
                    break;
                case hash("base64"):
                    this->decoder = new base64_decoder;
                    break;
                default:
                    printf("Unsported encoding: %s\n", transfer_encoding.c_str());
                    state = sync;
                    return 0;
                }
            }
        }
        state = file;
        return 0;
    }

    int section_t::close_object()
    {
        if (decoder != nullptr) {
            delete decoder;
            decoder = nullptr;
        }

        if (this->fp != nullptr) {
            fclose(fp);
            fp = nullptr;

            if (this->type == file && !this->filename.empty()) {
                if (!location.empty()) {
                    const std::string ending(".gif");
                    //            if (ending.size() < location.size() && std::equal(ending.rbegin(), ending.rend(), location.rbegin()));

                    if (location.length() >= ending.length() &&
                        (0 == location.compare(location.length() - ending.length(), ending.length(), ending)))
                    {
                        std::string filepath = "tmp/";
                        unsigned int po = location.find_last_of('/');
                        if (po != std::string::npos)
                            filepath += location.substr(po + 1);
                        else
                            filepath += location;

                        std::ifstream  src(this->filename.c_str(), std::ios::binary);
                        if (src.is_open()) {
                            std::ofstream  dst(filepath.c_str(), std::ios::binary);
                            if (dst.is_open()) {
                                dst << src.rdbuf();
                                dst.close();
                            }
                            else
                                printf("Unable create file %s\n", filepath.c_str());
                        }
                    }
                }
            }
        }
        return 0;
    }

    int section_t::destroy_object()
    {
        close_object();

        if (this->type == file && !this->filename.empty())
        {
            _unlink(this->filename.c_str());
            fprintf(stdout, "File '%s' has been deleted from the server's filesystem\n", this->filename.c_str());
        }
        return 0;
    }

    char* section_t::parse_transfer_enconding(char* value)
    {
        while (*value == ' ')
            value++;
        this->transfer_encoding = value;
        return value;
    }

    char* section_t::parse_location(char* value)
    {
        while (*value == ' ')
            value++;
        this->location = value;
        return value;
    }

    char* section_t::parse_content_type(char* value)
    {
        while (*value == ' ')
            value++;
        this->content_type = value;
        return value;
    }
    char* section_t::parse_content_disposition(char* line)
    {
        unsigned int i;
        position_type_t type;
        bool collect = true;
        char small_buff[64];
        char value[128];
        for (i = 0; collect && i < sizeof(small_buff);)
        {
            switch (line[i])
            {
            case '\0':
            case ';':
                small_buff[i] = 0;
                type = (position_type_t)hash(small_buff);
                collect = false;
                break;
            default:
                small_buff[i] = line[i];
                i++;
            }
        }

    loop:

        if (line[i] == ';')
        {
            while (line[++i] == ' ');

            unsigned int j = 0;

            for (collect = true; collect && j < sizeof(small_buff);)
            {
                switch (line[i])
                {
                case '\0':
                case ';':
                case '=':
                    small_buff[j] = 0;
                    type = (position_type_t)hash(small_buff);
                    collect = false;
                    break;
                default:
                    small_buff[j++] = line[i++];
                }
            }

            if (line[i] == ';')
                goto loop;

            if (line[i] == '=' && line[i + 1] == '"')
            {
                i += 2;
                char* dst = value;
                while (line[i] != '"')
                    *dst++ = line[i++];
                *dst = 0;
                i++;

                switch (type)
                {
                case Name:
                    this->name = value;
                    this->state = region::text;
                    break;

                case FileName:
                    this->value = value;
                    this->state = region::file;
                    break;

                default:
                    throw "Unparsed content format";
                    break;
                }

                if (line[i] == ';')
                    goto loop;
            }
        }
        return nullptr; // Never reach?
    }


    int parse_form_data(
        multipart_stream_reader_t   * reader,
        form_data_t* form_data,
        bool & out_sync)
    {
        int status = 0;
        //bool out_sync = false;
        bool ready = true;
        const char* p;
        section_t* section = new section_t;

        char header_name[128];
        char header_value[512];
        int name_size = 0;
        int value_size = 0;
        int bound_size;

        enum { enter_sync, second_sync, name_sync, value_sync } name_state;

        name_state = enter_sync;
        if (reader->get_state(bound_size) == multipart_stream_reader_t::Bound)
            section->type = region::header;

        int tmp_size = reader->get_rest();

        do {
            p = reader->get_pchar(out_sync, tmp_size);

            if (out_sync)
            {
                if (p == reader->bound)
                {
                    out_sync = false;

                    switch (section->type) {
                    case region::sync:
                        section->type = region::header;
                        name_size = 0;
                        value_size = 0;
                        name_state = enter_sync;
                        break;

                    case region::header:
                        printf("region::header\n");
                        break;

                    case region::text:
                        printf("region::text\n");
                        if (section->state != section_t::sync && section->state != section_t::header)
                        {
                            form_data->_objects.push_back(section);
                            section = new section_t;
                            section->type = region::header;
                        }
                        else {
                            fprintf(stderr, "Loose multipart section due sync error\n");
                        }
                        name_state = enter_sync;
                        break;
                    case region::file:
                        printf("region::file\n");
                        form_data->_objects.push_back(section);
                        section = new section_t;
                        section->type = region::header;
                        name_state = enter_sync;
                        break;
                    default:
                        throw "Unsupported content";
                    }
                }
                else if (p == reader->zero_socket)
                {
                    ready = false;
                    if (section->state != section_t::sync && section->state != section_t::header)
                    {
                        form_data->_objects.push_back(section);
                    }
                    else
                    {
                        fprintf(stderr, "Unexpected close socket\n");
                        status = -1;
                    }
                }
                else if (p == reader->error_socket)
                {
                    fprintf(stderr, p);
                    ready = false;
                }
                else if (p == nullptr)
                {
                    ready = false;
                }
                else
                    throw "Check me";
            }
            else if (section) switch (section->type) {
            case region::state_t::sync:
                break;

            case region::state_t::header:
                if (name_state == enter_sync) {
                    switch (*p)
                    {
                    case '-':
                        name_state = second_sync;
                        break;
                    case '\r':
                        name_state = second_sync;
                        break;
                        //case '\n':
                        //    name_state = name_sync;
                        //    break;
                    default:
                        throw "Sync error 1";
                    }
                }
                else if (name_state == second_sync) {
                    switch (*p)
                    {
                    case '-':
                        name_state = enter_sync;
                        ready = false;
                        break;
                    case '\n':
                        name_state = name_sync;
                        break;
                    default:
                        throw "Sync error 2";
                    }
                }
                else if (name_state == name_sync) {
                    if (*p == '\r')
                        ;
                    else if (*p == '\n')
                    {
                        section->create_object();
                    }
                    else if (*p != ':')
                        header_name[name_size++] = *p | 0b00100000;
                    else {
                        name_state = value_sync;
                        header_name[name_size++] = 0;
                        value_size = 0;
                    }
                }
                else if (*p == '\r')
                {
                    header_value[value_size] = 0;
                    //printf("Check delimiter - R\n");
                }
                else if (*p == '\n')
                {
                    char* s;
                    content_type_t prop_type = (content_type_t)hash(header_name);
                    printf("Check value on \\n %s: %s\n", get_property_name(prop_type), header_value);
                    switch (prop_type)
                    {
                    case ContentDisposition:
                        s = section->parse_content_disposition(header_value);
                        break;
                    case ContentType:
                        s = section->parse_content_type(header_value);
                        break;
                    case ContentTransferEncoding:
                        s = section->parse_transfer_enconding(header_value);
                        break;
                    case ContentLocation:
                        s = section->parse_location(header_value);
                        break;
                    default:
                        printf("Unparsed '%s' == %lx is '%s'\n", header_name, hash(header_name), header_value);
                    }

                    name_state = name_sync;
                    name_size = 0;
                }
                else
                {
                    if (name_size == 0 && *p == ' ')
                    {
                        printf("skip space after ':'\n");
                    }
                    else
                        header_value[value_size++] = *p;
                }
                break;
            case region::text:
                if (section->decoder == nullptr)
                {
                    section->value += *p;
                    section->size++;
                }
                else
                {
                    content_decoder_i::state_t     decoder_state;
                    char ch = section->decoder->stream(*p, decoder_state);
                    switch (decoder_state)
                    {
                    case content_decoder_i::state_t::data:
                        section->value += ch;
                        section->size++;
                        break;
                    case content_decoder_i::state_t::need_more:
                        break;
                    case content_decoder_i::state_t::have_more:
                        section->value += ch;
                        section->size++;
                        while (decoder_state == content_decoder_i::state_t::have_more)
                        {
                            section->value += section->decoder->stream(decoder_state);
                            section->size++;
                        }
                    }
                }
                break;
            case region::state_t::file:
                if (section->fp != nullptr)
                {
                    if (section->decoder == nullptr)
                    {
                        fputc(*p, section->fp);
                        section->size++;
                    }
                    else
                    {
                        content_decoder_i::state_t     decoder_state;
                        char ch = section->decoder->stream(*p, decoder_state);
                        switch (decoder_state)
                        {
                        case content_decoder_i::state_t::data:
                            section->value += ch;
                            fputc(ch, section->fp);
                            section->size++;
                            break;
                        case content_decoder_i::state_t::need_more:
                            break;
                        case content_decoder_i::state_t::have_more:
                            section->value += ch;
                            fputc(ch, section->fp);
                            section->size++;
                            while (decoder_state == content_decoder_i::state_t::have_more)
                            {
                                ch = section->decoder->stream(decoder_state);
                                fputc(ch, section->fp);
                                section->value += ch;
                                section->size++;
                            }
                        }
                    }
                }
                break;
            default:
                throw "Unsupported content";
            }
        } while (ready);
        if (section->state == section_t::sync)
            delete section;
        return status;
    }


    void show(form_data_t* form_data)
    {
        printf("Show form data [%lu]:\n", (long unsigned int) form_data->_objects.size());
        for (auto const& section : form_data->_objects) {
            switch (section->type)
            {
            case region::text:
                printf("Input: '%s' Len: %u Value: '%s'\n",
                    section->name.c_str(),
                    section->size,
                    section->value.c_str()
                );
                break;
            case region::file:
                printf("File: '%s' Size: %u Tmp_name: '%s'\n",
                    section->value.c_str(),
                    section->size,
                    section->filename.c_str());
                break;
            default:
                printf("Not-Parsed type: %d name: '%s'\n", section->type, section->name.c_str());
                break;
            }
        }
    }
}

///
/// Here is test suite 
///
#if false

//#define FILENAME    "raw/post.bin"
//#define FILENAME    "raw/p.mhtml"
#define FILENAME    "raw/rcv_sock-1.raw"

extern xameleon::transport_t* create_file_descriptor_transport(FILE* fp);


int test_multipart()
{
    xameleon::form_data_t   form_data("test_test");
    const char* bound = "----WebKitFormBoundaryfTAFOsEH3u7AGlmQ";
    const char* form_bound = "---------------------------47781317411613541952031516410";
    const char* web_bound = "----MultipartBoundary--sUQZHThEQ6CF0B1hRpSLP34AhlmaQ1SWG8whIfTTw6----";

    FILE* fp = fopen(FILENAME, "rb");
    if (fp == NULL)
    {
        perror("Unable open multipart file");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //xameleon::FileTransport transport(fp);
    xameleon::transport_t* transport = create_file_descriptor_transport(fp);
    xameleon::multipart_stream_reader_t* reader = new xameleon::multipart_stream_reader_t(transport, 0, nullptr);
    
    reader->set_content_length(size);
    reader->set_bound(form_bound);

    int r = parse_form_data(reader, &form_data);

    delete reader;
    delete transport;

    fclose(fp);

    form_data.close_temp_files();

//    void show(form_data_t * form_data);
    xameleon::show(&form_data);

    return (r > 0) ? 0 : 1;
}

#endif