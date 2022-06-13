#include "session_mgr.h"

session_mgr_t::session_mgr_t()
{
    if (pthread_mutex_init(&lock_session_list, NULL) != 0) {
        perror("mutex init failed");
        exit(1);
    }
}
session_mgr_t::~session_mgr_t()
{
    pthread_mutex_destroy(&lock_session_list);
}
int session_mgr_t::add_session(https_session_t* session)
{
    port_map_t  * ports = nullptr;
    pthread_mutex_lock(&lock_session_list);
    do {
        const sockaddr* const addr = session->get_transport()->address();
        if (addr->sa_family != AF_INET) {
            fprintf(stderr, "This version supports IPv4 only\n");
            break;
        }
        const struct sockaddr_in* const V4 = reinterpret_cast<const sockaddr_in* const>(addr);
        unsigned long ip = V4->sin_addr.s_addr;
        if (active_sessions.count(ip) == 0) {
            active_sessions[ip] = new port_map_t;
            printf("Add new IP address & ");
        }
        else {
            printf("Use known IP adrress & ");
        }
        ports = (active_sessions)[ip];
        unsigned short pr = V4->sin_port;
        if (ports->count(pr) != 0) {
            fprintf(stderr, "Session with same ID already present");
            break;
        }
        (*ports)[pr] = session;
        printf("Session added to list of active sessions\n");
    } while (false);
    pthread_mutex_unlock(&lock_session_list);
    return 0;
}
int session_mgr_t::remove_session(https_session_t* session)
{
    pthread_mutex_lock(&lock_session_list);
    do {
        const sockaddr* const addr = session->get_transport()->address();
        if (addr->sa_family != AF_INET) {
            fprintf(stderr, "This version supports IPv4 only\n");
            break;
        }
        const struct sockaddr_in* const V4 = reinterpret_cast<const sockaddr_in* const>(addr);
        unsigned long ip = V4->sin_addr.s_addr;
        //ip4_map_t::iterator 
        if (active_sessions.count(ip) == 0) {
            fprintf(stderr, "Unable remove session: IP adress not found\n");
            break;
        }
        port_map_t* ports = (active_sessions)[ip] ;
        unsigned short pr = V4->sin_port;
        if (ports->count(pr) == 0) {
            fprintf(stderr, "Unable remove session: source port not found\n");
            break;
        }
//        https_session_t* session = (*ports)[pr]; // ports[ipV4->sin_port];
        ports->erase(pr);
//        delete session;
        printf("Session removed form list of active sessions\n");
    } while (false);
    pthread_mutex_unlock(&lock_session_list);
    return 0;
}
