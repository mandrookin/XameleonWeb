#pragma once

#ifndef _WIN32
#include <pthread.h>
#else
#include <windows.h>
#endif


namespace xameleon
{
    class Mutex {
#ifndef _WIN32
        pthread_mutex_t  critical_section;
#else
        CRITICAL_SECTION  critical_section;
#endif
    public:
        void Lock();
        void Unlock();

        Mutex();
        ~Mutex();
    };

}
