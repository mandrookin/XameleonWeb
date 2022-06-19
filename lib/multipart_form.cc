#define _CRT_SECURE_NO_WARNINGS 1
#define DEBUG_FORM
//#define DEBUG_MSVS

// Если определить, компилятор ругается на опасную устаревшую функцию
//#define TMPNAM 1

// 中国

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/fcntl.h>

#include "../transport.h"
#include "../multipart_form.h"

region::~region()
{
    if (this->type == file && ! this->filename.empty()) {
        unlink(this->filename.c_str());
        fprintf(stdout, "File '%s' has been deleted from the server's filesystem\n", this->filename.c_str());
    }
    fprintf(stdout, "Delete section %s\n", name.c_str());
}

form_data_t::form(const char * name)
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

// Алгоритм не отработает, если размер буфера меньше 256 байт.
static const int BUFSIZE = 16 * 1024; // 256; // 512; // 
static const int CACHE_SIZE = 16 * 1024;

static int get_file_name(FILE* fp, char* namebuf, int bufsize)
{
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
    return nr;
}

int parse_form_data(transport_t* fp, int rest_size, const char* bound, form_data_t* form_data)
{
    section_t* section = new section_t;
    char            namebuf[L_tmpnam];
    char            iobuf[CACHE_SIZE];
    char            buf[BUFSIZE];

    //    printf("FILE Buffer size = %u, name fuffer size %u\n", BUFSIZ, L_tmpnam);
//    FILE* tmp = fopen("sform.bin", "wb");

    const char* prefix = "Content-Disposition: form-data; ";
    const char* ctype = "Content-Type: ";
    const char* term = "\r\n\r\n";

    int bound_len = std::strlen(bound) - 2;
    int match_rest = bound_len;
    const char* pbound = bound + 2;;
    int rd_len;

    while (rest_size > 0) {
//        rd_len = fread(buf, 1, BUFSIZE, fp);
        rd_len = fp->recv(buf, BUFSIZE);
        if (rd_len <= 0) {
            fprintf(stderr, "Unable read data");
            break;
        }

//        fwrite(bug, rd_len, 1, tmp);

        rest_size -= rd_len; // Это должно быть выше цикла, потому что rd_len изменяется ниже
        char* src = buf;

        do {
            int save_len = rd_len;
            int i = 0;
            while (match_rest > 0 && rd_len > 0) {
                int idx = bound_len - match_rest;
                if (src[i++] == pbound[idx]) {
                    match_rest--;
                    rd_len--;
                    continue;
                }

                if (rd_len == 0)
                    break;

                if (match_rest == 2 && src[idx] == '-' && src[idx + 1] == '-') {
//                    printf("Finished. Type is %d\n", section->type);
                    if (section->size != 0) {
                        if (section->fp != nullptr) {
                            fclose(section->fp);
                            section->fp = nullptr;
                        }
                        form_data->_objects.push_back(section);
                        section = nullptr;
                    }
                    rd_len -= 4;
                    break;
                }
                else switch (section->type)
                {
                case region::sync:
                    break;
                case region::text:
                    section->value += *src;
                    break;
                case region::file:
#ifndef DEBUG_MSVS
                    fputc(*src, section->fp);
#else
                    section->content += *src;
#endif
                    break;
                }
                section->size++;
                src++;
                rd_len = --save_len;
                match_rest = bound_len;
                i = 0;
            }

//            printf("Secton type = %d Value='%s'\n", section->type, section->value.c_str());

            if (rd_len == 0) // А может быть match_rest != 0 ? Да без разницы! 
            {
//                printf("RD_LEN is zero. Rest size = %d\n\n", rest_size);
                break;
            }

            src += i;
            if (rd_len < 256 && rest_size > 0) {
//                printf("Weakpoint detected: rd_len %d rest_size %d\n", rd_len, rest_size);
                                // Пробую самый лёгкий алгоритм, поскольку лепить сложную стейт-машину нет ни сил, ни желания, ни времени
                std::memcpy(buf, src, rd_len);
//                int chunk_len = fread(buf + rd_len, 1, BUFSIZE - static_cast<size_t>(rd_len), fp);
                int chunk_len = fp->recv(buf + rd_len, BUFSIZE - static_cast<size_t>(rd_len));
                src = buf;
                rd_len += chunk_len;
                rest_size -= chunk_len;
            }

//          printf("Section type %u '%s'\n", section->type, section->name.c_str());

            if (section->type == region::sync) {
                pbound = bound;
                bound_len += 2;
            }
            else {
                if (section->size != 0) {
                    form_data->_objects.push_back(section);
#ifndef DEBUG_MSVS
                    if (section->fp != nullptr) {
                        fclose(section->fp);
                        section->fp = nullptr;
                    }
#endif
                }
                else {
                    delete section;
                }
                section = new section_t;
            }

            match_rest = bound_len;

            char* hdr_ptr = src;
            const char* p = prefix;
            while (*p) {
                if (*p != *src)
                    goto formerror;
                p++;
                src++;
            }

//            char * pname = src;
            while (*src != '=') src++;
            *src++ = 0;
            if (*src++ != '"')
                goto formerror;

            while (*src != '"')
                section->name += *src++;

//            printf("pname found %s\n", pname);
//            char * fname = nullptr;

            src++;

            if (*src == ';') {
                section->type = region::file;
                *src = '\0';
                src++;
                while (*src == ' ') src++;
//                fname = src;
                while (*src != '=') src++;
                *src++ = 0;
                if (*src++ != '"')
                    goto formerror;
                while (*src != '"')
                    section->value += *src++;
                src++;
                if (src[0] != '\r' || src[1] != '\n')
                    goto formerror;
                src += 2;
                if(!section->value.empty()) {
#if TMPNAM
                    char * nowarn = tmpnam(namebuf);
                    printf("%s\n", nowarn);
                    section->filename = namebuf;
#else
                    section->fp = tmpfile();
#endif
                }
                p = ctype;
                while (*p) {
                    if (*p != *src)
                        goto formerror;
                    p++;
                    src++;
                }
                while (*src != '\r')
                    section->content_type += *src++;
#ifndef DEBUG_MSVS
                if (!section->value.empty()) {
#if TMPNAM
                    section->fp = fopen(section->filename.c_str(), "wb");
#else
                    int status = get_file_name(section->fp, namebuf, L_tmpnam);
                    if (status < 0) {

                    }
                    section->filename = namebuf;
#endif
                    setvbuf(section->fp, iobuf, _IOFBF, CACHE_SIZE);
                    fprintf(stdout, "Create file '%s' Filename: %s\n", section->filename.c_str(), section->value.c_str());
                }
#endif
            }
            else {
//                fprintf(stdout, "Create input '%s'\n", section->name.c_str());
                if (*src != '\r')
                    goto formerror;
                section->type = region::text;
            }
//            printf("Looking terminator\n");
            p = term;
            while (*p) {
                if (*p != *src)
                    goto formerror;
                p++;
                src++;
            }
//            printf("Before data\n");
            rd_len -= (src - hdr_ptr);
            if (rd_len < 0) {
                goto formerror;
            }
//            printf("About begin data: rd_len = %d = (%p - %p)\n", rd_len, src, hdr_ptr);
        } while (rd_len > 0);
    }
//    fclose(tmp);
    if(section != nullptr)
        delete section;
    return 0;
formerror:
//    fclose(tmp);
    fprintf(stderr, "Form format error\nDetected Bound: %s\nBuffer [0-%u]:\n%s\n\n", bound, (unsigned)sizeof(buf), buf);
    return -1;
}
#if false

int test_main()
{
    const char* bound = "\r\n------WebKitFormBoundaryfTAFOsEH3u7AGlmQ\r\n";

    FILE* fp = fopen("post.bin", "rb");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int r = parse_form_data(fp, size, bound);
    fclose(fp);

    void show();

    return (r > 0) ? 0 : 1;
}
#endif

void show(form_data_t * form_data)
{
    printf("Show form data [%lu]:\n", form_data->_objects.size());
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