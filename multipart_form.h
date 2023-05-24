#pragma once
#include <string>
#include <stdio.h>
#include <list>
//#include "transport.h"
#include "session.h"
#include "lib/multipart_stream_reader.h"

#ifndef _WIN32
#include <cstring>
#endif

namespace xameleon
{

    class content_decoder_i {
    public:
        typedef enum { data, need_more, have_more } state_t;
        virtual char stream(const char in, state_t& need_more) = 0;
        virtual char stream(state_t& need_more) = 0;
        virtual ~content_decoder_i() { ; }
    };

    typedef struct region {
        typedef enum {
            sync,
            header,
            text,
            file
        } state_t;

        typedef enum {
            octet_stream = hash("application/octet-stream"), // 0x62c58c5d,
            text_xml = hash("text/xml"),
            text_html = hash("text/html"),
            text_plain = hash("text/plain"),
            text_css = hash("text/css"),
            image_gif = hash("image/gif"),
            image_png = hash("image/png"),
            image_jpeg = hash("image/jpg")
        } contect_type_t;

        typedef enum {
            none,
            quoted_printable = 0xeb55b907,
            base64 = 0x1d196aa7,
        } transfer_encoding_t;

        state_t         state;
        state_t         type;
        std::string     name;
        std::string     value;
        std::string     filename;
        std::string     content_type;
        std::string     location;
        std::string     transfer_encoding;
        FILE* fp;
        unsigned int    size;
        transfer_encoding_t encoding;
        content_decoder_i* decoder;

//#ifdef DEBUG_MSVS
//        std::string     content;
//#endif
        char* parse_location(char* value);
        char* parse_transfer_enconding(char* value);
        char* parse_content_disposition(char* value);
        char* parse_content_type(char* value);
        region()
        {
            state = sync;
            type = sync; size = 0; fp = nullptr;
            encoding = none; decoder = nullptr;
        }
        ~region();

        int create_object();
        int close_object();
        int destroy_object();
    } section_t;

    typedef struct form {
        std::string _name;
        std::list<section_t*>   _objects;
        form(const char* name);
        ~form();
        int close_temp_files();
    } form_data_t;

    void show(form_data_t* form_data);

    int parse_form_data(
        multipart_stream_reader_t* reader,
        form_data_t* form_data,
        bool & outsync);
}