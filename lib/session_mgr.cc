#include "../session_mgr.h"

#ifndef _WIN32
#include "../db/ipv4_log.h"
#endif

namespace xameleon 
{
#ifndef _WIN32
    static ipv4_log         *   log = nullptr;
#endif
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
        server_start_time = time(nullptr);
#ifndef _WIN32
        DB* r = nullptr;
        if (log == nullptr) {
            log = get_ip_database();
            if (log)
                r = log->create_ip_database();
        }
        if (!r)
            fprintf(stderr, "Unable create IP-adress database\n");
#endif
    }
    session_mgr_t::~session_mgr_t()
    {
#ifndef _WIN32
        if (log != nullptr)
            log->release_ip_database();
#endif
    }
    int session_mgr_t::add_session(https_session_t* session)
    {
        port_map_t* ports = nullptr;
        lock_list.Lock();
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
        lock_list.Unlock();
        return 0;
    }
    int session_mgr_t::remove_session(https_session_t* session)
    {
        lock_list.Lock();
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
                printf("Session found\n");
#ifdef WIN32
#else
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
#endif
            }
            else
                printf("Session not found\n");

                
            ports->erase(pr);
            //        delete session;
        } while (false);
        lock_list.Unlock();
        return 0;
    }

    int session_mgr_t::touch_page(url_t& url)
    {
	// Поместить это в критическую секцию
        pageinfo_t& page = touch_list[url.path];
	if(page.first == 0)
           page.first = time(nullptr);
        page.access_count++;
        page.last = time(nullptr);
        return 0;
    }
}