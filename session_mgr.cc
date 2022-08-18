#include "session_mgr.h"
#include "db/ipv4_log.h"

namespace xameleon 
{
    static ipv4_log         *   log = nullptr;
    static time_t               server_start_time = 0;
    static session_mgr_t        session_cache;

    // Возвращает количество секунд с момента старта программы
    long long uptime()
    {
        time_t now = time(nullptr);
        return now - server_start_time;
    }

    session_mgr_t* get_active_sessions()
    {
        return &session_cache;
    }


    session_mgr_t::session_mgr_t()
    {
        if (pthread_mutex_init(&lock_session_list, NULL) != 0) {
            perror("mutex init failed");
            exit(1);
        }
        server_start_time = time(nullptr);
        DB* r = nullptr;
        if (log == nullptr) {
            log = get_ip_database();
            if (log)
                r = log->create_ip_database();
        }
        if (!r)
            fprintf(stderr, "Unable create IP-adress database\n");
    }
    session_mgr_t::~session_mgr_t()
    {
        if (log != nullptr)
            log->release_ip_database();

        pthread_mutex_destroy(&lock_session_list);
    }
    int session_mgr_t::add_session(https_session_t* session)
    {
        port_map_t* ports = nullptr;
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
            uint32_t ip = V4->sin_addr.s_addr;
            //ip4_map_t::iterator 
            if (active_sessions.count(ip) == 0) {
                fprintf(stderr, "Unable remove session: IP adress not found\n");
                break;
            }
            port_map_t* ports = (active_sessions)[ip];
            unsigned short pr = V4->sin_port;
            if (ports->count(pr) == 0) {
                fprintf(stderr, "Unable remove session: source port not found\n");
                break;
            }

            https_session_t* session = (*ports)[pr]; // ports[ipV4->sin_port];
            if(session != nullptr)
            {
                http_session_counters_t counter = session->get_counters();
                ipv4_record_t record;

                int status = log->load_record(ip, &record);
                if (status == DB_NOTFOUND) {
                    record.ip = (int32_t) ip;
                    record.first_seen = time(nullptr);
                    record.last_seen = record.first_seen;
                    record.counters = counter;
                }
                else if (status == 0) {
                    printf("FOUND SESSION IP %08x == %08x\n", ip, record.ip );
                    record.last_seen = time(nullptr);
                    record.counters += counter;
                }
                log->save_record(&record);

                if (session->request.x_forward_for != 0 || session->request.x_real_ip != 0)
                {
                    if (session->request.x_forward_for != session->request.x_real_ip)
                        printf("Denug: x_forward_for = 0x%08x x_real_ip = 0x%08x\n", session->request.x_forward_for, session->request.x_real_ip);

                    if (session->request.x_forward_for != 0)
                    {
                        int status = log->load_record(session->request.x_forward_for, &record);
                        if (status == DB_NOTFOUND) {
                            record.ip = (int32_t)session->request.x_forward_for;
                            record.first_seen = time(nullptr);
                            record.last_seen = record.first_seen;
                            record.counters = counter;
                        }
                        else if (status == 0) {
                            printf("FOUND FORWARD IP %08x == %08x\n", session->request.x_forward_for, record.ip);
                            record.last_seen = time(nullptr);
                            record.counters += counter;
                        }
                        log->save_record(&record);
                    }
                }
            }
                
            printf("Session found, ip database updated, it will removed form list of active sessions\n");
            ports->erase(pr);
            //        delete session;
        } while (false);
        pthread_mutex_unlock(&lock_session_list);
        return 0;
    }
}