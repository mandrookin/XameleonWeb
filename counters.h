#pragma once

namespace xameleon 
{
    class http_session_counters_t {
    public:
        int                 total_request;
        int                 x_forward_counters;
        int                 x_realip_counters;
        int                 bad_request;
        int                 not_found;
        int                 http2https;
        http_session_counters_t() :
            total_request(0), x_forward_counters(0), x_realip_counters(0), bad_request(0), not_found(0), http2https(0)
        {
        }

        http_session_counters_t& operator+= (http_session_counters_t const& right)
        {
            (*this).total_request += right.total_request;
            (*this).x_forward_counters += right.x_forward_counters;
            (*this).x_realip_counters += right.x_realip_counters;
            (*this).bad_request += right.bad_request;
            (*this).not_found += right.not_found;
            (*this).http2https += right.http2https;
            return *this;
        }

    };
}