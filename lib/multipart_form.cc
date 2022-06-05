#define _CRT_SECURE_NO_WARNINGS 1
#define DEBUG_FORM
#define DEBUG_MSVS

// 中国

#include <stdio.h>
#include <string>
#include <cstring>
#include <list>

typedef struct region {
    typedef enum {
        sync,
        text,
        file
    } state_t;

    state_t         type;
    std::string     name;
    std::string     value;
    std::string     filename;
    std::string     content_type;
    FILE          * fp;
    unsigned int    size;
#ifdef DEBUG_MSVS
    std::string     content;
#endif
    region() { type = sync; size = 0; fp = nullptr; }
} section_t;

std::list<section_t*>   received_objects;

// Алгоритм не отработает, если размер буфера меньше 256 байт.
static const int BUFSIZE = 16 * 1024; // 256; // 512; // 
static const int CACHE_SIZE = 16 * 1024;

int parse_form_data(FILE * fp, int rest_size, const char * bound)
{
    section_t   *   section = new section_t;
    char            namebuf[L_tmpnam];
    char            iobuf[CACHE_SIZE];
    char            buf[BUFSIZE];

    //    printf("FILE Buffer size = %u, name fuffer size %u\n", BUFSIZ, L_tmpnam);

    const char * prefix = "Content-Disposition: form-data; ";
    const char * ctype = "Content-Type: ";
    const char * term = "\r\n\r\n";

    int bound_len = std::strlen(bound) - 2;
    int match_rest = bound_len;
    const char * pbound = bound + 2;;
    int rd_len;

    while (rest_size > 0) {
        rd_len = fread(buf, 1, BUFSIZE, fp);
        if (rd_len <= 0) {
            fprintf(stderr, "Unable read data");
            break;
        }

        rest_size -= rd_len; // Это должно быть выше цикла, потому что rd_len изменяется ниже
        char * src = buf;

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

                if (match_rest == 2 && src[idx] == '-' && src[idx+1] == '-') {
                    printf("Finished\n");
                    received_objects.push_back(section);
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

            if (rd_len == 0) // А может быть match_rest != 0 ? Да без разницы!
                break;

            src += i;
            if (rd_len < 256) {
//                printf("Weakpoint detected\n");
                // Пробую самый лёгкий алгоритм, поскольку лепить сложную стейт-машину нет ни сил, ни желания, ни времени
                std::memcpy(buf, src, rd_len);
                int chunk_len = fread(buf + rd_len, 1, BUFSIZE - static_cast<size_t>(rd_len), fp);
                src = buf;
                rd_len += chunk_len;
                rest_size -= chunk_len;
            }

            if (section->type == region::sync) {
                pbound = bound;
                bound_len += 2;
            }
            else {
                received_objects.push_back(section);
                section = new section_t;
#ifndef DEBUG_MSVS
                if (section->fp != nullptr) {
                    fclose(section->fp);
                    section->fp = nullptr;
                }
#endif
            }

            match_rest = bound_len;

            char * hdr_ptr = src;
            const char * p = prefix;
            while (*p) {
                if (*p != *src)
                    goto formerror;
                p++;
                src++;
            }

            char * pname = src;
            while (*src != '=') src++;
            *src++ = 0;
            if (*src++ != '"')
                goto formerror;

            while (*src != '"')
                section->name += *src++;

            char * fname = nullptr;
            src++;

            if (*src == ';') {
                section->type = region::file;
                *src = '\0';
                src++;
                while (*src == ' ') src++;
                fname = src;
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
                tmpnam(namebuf);
                section->filename = namebuf;

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
                section->fp = fopen(section->filename.c_str(), "wb");
                setvbuf(section->fp, iobuf, _IOFBF, CACHE_SIZE);
#endif
            }
            else {
                if (*src != '\r')
                    goto formerror;
                section->type = region::text;
            }
            p = term;
            while (*p) {
                if (*p != *src)
                    goto formerror;
                p++;
                src++;
            }
            rd_len -= (src - hdr_ptr);
            if (rd_len < 0) {
                goto formerror;
            }
//            printf("About begin data\n");
        } while (rd_len > 0);
    }
    return 0;
formerror:
    fprintf(stderr, "Form format error\n");
    return -1;
}

int test_main()
{
    const char* bound = "\r\n------WebKitFormBoundaryfTAFOsEH3u7AGlmQ\r\n";

    FILE* fp = fopen("post.bin", "rb");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int r = parse_form_data(fp, size, bound);
    fclose(fp);

    for (auto const& section : received_objects) {
        switch (section->type)
        {
        case region::text:
            printf("Input: '%s' Value: '%s' Len: %u\n",
                section->name.c_str(),
                section->value.c_str(),
                section->size);
            break;
        case region::file:
            printf("File: '%s' Size: %u Tmp_name: '%s'\n",
                section->value.c_str(),
                section->size,
                section->filename.c_str());
            break;
        }
    }

    return 0;
}
