#pragma once

namespace xameleon
{
// Загружает файл в память и возвращает указаль на образ файла в оперативной памяти
char * alloc_file(const char* filename, int* size);

// Возвращает время в секундах с момент старта https сервера
long long uptime();

}