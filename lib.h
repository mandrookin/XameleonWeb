#pragma once

#include "http.h"

#ifndef _WIN32
#else
#include <windows.h>
#endif

namespace xameleon
{
    // Загружает файл в память и возвращает указаль на образ файла в оперативной памяти
    char* alloc_file(const char* filename, int* size);

    // Возвращает время в секундах с момент старта https сервера
    long long uptime();

    inline const char* http_method(http_method_t method);

}