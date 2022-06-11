#pragma once

#include <pthread.h>
#include <map>
#include "session.h"

class session_mgr_t
{
    pthread_mutex_t  lock_session_list;
public:
    typedef std::map<unsigned short, https_session_t *> port_map_t;
    typedef std::map<unsigned long int, port_map_t * > ip4_map_t;
    ip4_map_t    active_sessions;
    session_mgr_t();
    ~session_mgr_t();
    int add_session(https_session_t* session);
    int remove_session(https_session_t* session);
};