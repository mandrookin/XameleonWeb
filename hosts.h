#pragma once
#include <string>
#include <map>

namespace xameleon
{
    typedef enum {
        pages_and_cgi,
        proxy
    } mode_t;

    typedef struct host_s {
        mode_t          mode;
        std::string		hostname;
        int			    port;
        std::string		host_and_port;
        std::string		home;
        int             access_count;
        int             http_session_maxtime;
        host_s() : port(80), access_count(0), http_session_maxtime(0) {}
  } host_t;

  typedef std::map<std::string, host_t> hostmap_t;

  host_t* find_host(const char * host, int port);
  host_t* find_host(const char* key);
}