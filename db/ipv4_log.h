#ifndef _WIN32
#include <db.h>
#endif

#include "../counters.h"

namespace xameleon {

    typedef struct {
        int32_t                     ip;
        time_t                      first_seen;
        time_t                      last_seen;
        http_session_counters_t     counters;
        int                         country_code; // Perhaps phone number или типа того
    } ipv4_record_t;

    typedef int (*ip_list_callback_t)(void * obj, ipv4_record_t *);

#ifndef _WIN32

    class ipv4_log
    {
        DB_ENV* envp = NULL;
        DB* dbp = NULL;
    public:
        ipv4_log();
        ~ipv4_log();
        int load_record(int32_t ip, ipv4_record_t* value);
        int save_record(ipv4_record_t* value);
        int list(int from, int count, ip_list_callback_t cb, void * obj);
        int show_database();
        int test();
        DB* create_ip_database();
        int release_ip_database();
        static int sprint_ip(char* str, uint32_t ip);
    };

    ipv4_log* get_ip_database();
#endif

}

