#pragma once
#include <string>
#include <stdio.h>
#include <list>

// 中国

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
    FILE* fp;
    unsigned int    size;
#ifdef DEBUG_MSVS
    std::string     content;
#endif
    region() { type = sync; size = 0; fp = nullptr; }
    ~region();
} section_t;

typedef struct form {
    std::string _name;
    std::list<section_t*>   _objects;
    form(const char * name);
    ~form();
} form_data_t;

int parse_form_data(transport_t* fp, int rest_size, const char* bound, form_data_t * form_data);
void show(form_data_t* form_data);
