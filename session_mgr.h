#pragma once

#include "mutex.h"
#include <map>
#include <string.h>
#include "session.h"

namespace xameleon {

    typedef struct pafeinfo_s
    {
        int access_count;
	time_t first;
	time_t last;
        pafeinfo_s() : access_count(0), first(0), last(0) {}
    } pageinfo_t;

    class session_mgr_t
    {
        Mutex            lock_list;
    public:
        typedef std::map<unsigned short, https_session_t*> port_map_t;
        typedef std::map<unsigned long int, port_map_t* > ip4_map_t;
        typedef std::map<std::string, pageinfo_t> touch_list_t;
        ip4_map_t           active_sessions;
        touch_list_t        touch_list;
        session_mgr_t();
        ~session_mgr_t();
        int add_session(https_session_t* session);
        int remove_session(https_session_t* session);
        int touch_page(url_t& url);
    };

    long long uptime();
    session_mgr_t* get_active_sessions();
}
