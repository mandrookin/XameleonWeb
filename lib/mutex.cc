#include "../mutex.h"
#include <stdio.h>
#include <stdlib.h>

namespace xameleon
{
        void Mutex::Lock()
        {
#ifndef _WIN32
            pthread_mutex_lock(&critical_section);
#else
            EnterCriticalSection(&critical_section);
#endif
        }
        void Mutex::Unlock()
        {
#ifndef _WIN32
            pthread_mutex_unlock(&critical_section);
#else
            LeaveCriticalSection(&critical_section);
#endif
        }
        Mutex::Mutex() {
#ifndef _WIN32
            if (pthread_mutex_init(&critical_section, NULL) != 0) {
                perror("mutex init failed");
                exit(1);
            }
#else
            InitializeCriticalSection(&critical_section);
#endif
        }
        Mutex::~Mutex()
        {
#ifndef _WIN32
            pthread_mutex_destroy(&critical_section);
#else
            DeleteCriticalSection(&critical_section);
#endif
        }
}