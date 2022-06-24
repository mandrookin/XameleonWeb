#pragma once

namespace xameleon 
{
    typedef struct session_counters {
        int                 total_request;
        int                 bad_request;
        int                 not_found;
        int                 http2https;
        session_counters() : total_request(0), bad_request(0), not_found(0), http2https(0) {}
    } http_session_counters_t;
}