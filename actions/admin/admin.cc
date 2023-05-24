#define _CRT_SECURE_NO_WARNINGS 1
#include <string.h>
#include <sstream>

#include "../../action.h"
#include "../../lib.h"
#include "../../session.h"
#include "../../session_mgr.h"
#include "../../stream_writer.h"
#include "../../db/ipv4_log.h"


#ifndef _WIN32
#include <sys/sysinfo.h>
#define O_BINARY 0
 #include <sys/stat.h>
 #include <fcntl.h>

namespace xameleon {
    extern session_mgr_t* get_active_sessions();
}

#else
#include <chrono>
#endif



// Полезное
// https://stackoverflow.com/questions/9975810/make-iframe-automatically-adjust-height-according-to-the-contents-without-using

namespace xameleon {

    static const char* admin_style =
        "<style>"
            "table, td {"
                "border: 1px solid black;"
                "background-color: white;"
                "border-spacing: 0;"
                "border-collapse: collapse;"
                "text-align: center;"
                "vertical-align: middle;"
            "}"
            "th {"
                "border: 1px solid black;"
        "background-color: rgb(240, 240, 240);"
                "border-spacing: 0;"
                "border-collapse: collapse;"
            "}"
        "</style>";

    typedef struct ip_context {
        stream_writer*    fp;
        int             counter;
        ip_context() : fp(nullptr), counter(0) {}
        ip_context(https_session_t* session) : counter(0) 
        {
            fp = new stream_writer(session);
        }
        ~ip_context()
        {
            if (fp != nullptr) delete fp;
        }
    } ip_context_t;

    static int print_uptime(char* str, long long sec)
    {
        int sz = 0;
        int s = (int)(sec % 60);
        sec /= 60;
        int m = (int)(sec % 60);
        sec /= 60;
        int h = (int)(sec % 24);
        sec /= 24;
        int d = (int)(sec);
        if (d) {
            sz += snprintf(str, 64, "%d days ", d);
        }
        if (h) {
            sz += snprintf(str + sz, 64, "%d hour(s) ", h);
        }
        if (m) {
            sz += snprintf(str + sz, 64, "%d minutes ", m);
        }
        sz += snprintf(str + sz, 64, "%d second(s)\n", s);
        return sz;
    }

    
    static char* const generate_references_page(https_session_t* session)
    {
        url_t* url = &session->request.url;
        http_response_t* response = &session->response_holder;
        /// ПЕРЕДЕЛАТЬ НА buffered_send !!!!
        char buffer[16 * 1024];

        response->_code = 200;
        response->_content_type = "text/plain; charset=utf-8";
        response->_body_size = 0;

        auto & pages_map = get_active_sessions()->touch_list;
        for (auto& page : pages_map) {
            char first_time[32];
            char last_time[32];
            strftime(first_time, 32, "%Y-%m-%d %H:%M:%S", localtime(&page.second.first));
            strftime(last_time, 32, "%Y-%m-%d %H:%M:%S", localtime(&page.second.last));
            response->_body_size += snprintf(buffer + response->_body_size, sizeof(buffer) - response->_body_size,
                "%-40s %6d  %s   %s\r\n", page.first.c_str(), page.second.access_count, first_time, last_time);
        }

        response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);
        response->_body = buffer;
        session->get_transport()->send(response->_header, response->_header_size);
        session->get_transport()->send(response->_body, response->_body_size);
        return nullptr;
    }

    static int ip_list_callback(void* obj, ipv4_record_t* ipv4)
    {
        ip_context_t* context = (ip_context_t*)obj;
        char first[128], last[128], buf[1024];
#if false  // ndef _WIN32
        ipv4_log::sprint_ip(buf, ipv4->ip);
#else
        uint32_t ip = ipv4->ip;
        snprintf(buf, sizeof(buf), "<a href=\"https://ru.infobyip.com/ip-%u.%u.%u.%u.html\" target=\"_blank\">%u.%u.%u.%u</a>", 
            0xff & ip, 0xff & (ip >> 8), 0xff & (ip >> 16), ip >> 24, 
            0xff & ip, 0xff & (ip >> 8), 0xff & (ip >> 16), ip >> 24 );
#endif
        strftime(first, 20, "%Y-%m-%d %H:%M:%S", localtime(&ipv4->first_seen));
        strftime(last, 20, "%Y-%m-%d %H:%M:%S", localtime(&ipv4->last_seen));

        context->fp->sprintf(false, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>\n",
            buf, first, last,
            ipv4->counters.total_request,
            ipv4->counters.bad_request,
            ipv4->counters.not_found,
            ipv4->counters.http2https);
        context->counter++;
        return 0;
    }

    static void generate_ipv4_list(https_session_t* session)
    {
//        http_request_t* request = &session->request;
        http_response_t* response = &session->response_holder;
        int sz;
        ip_context_t context(session);

        response->_content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, 200, 0);
        sz = context.fp->send(false, response->_header, response->_header_size);

        {
            char  server_period[128];
            char  service_period[128];
#ifndef _WIN32
            struct sysinfo      info;
            int status = sysinfo(&info);
            if (status == 0)
                print_uptime(server_period, info.uptime);
#else
            auto uptime = std::chrono::milliseconds(GetTickCount64());
            print_uptime(server_period, uptime.count() / 1000);
#endif
            print_uptime(service_period, xameleon::uptime());
            context.fp->sprintf(false,
                "<html lang='utf8'>"
                "%s"
                "<body>"
                "<p>System uptime: %s<br/>HTTPS service uptime: %s<br/></p>",
                admin_style, server_period, service_period);

#ifndef _WIN32
            ipv4_log* ip_db = get_ip_database();
            if (ip_db) {
                context.fp->sprintf(false,
                    "<h3>Статистика IP адресов с момента старта сервера</h3><br/>\n"
                    "<table><tr>"
                        "<th style='width: 160px'>IP адрес</th>"
                        "<th style='width: 160px'>Первый визит</th>"
                        "<th style='width: 160px'>Предыдущий визит</th>"
                        "<th style='width: 80px'>Total</th>"
                        "<th style='width: 80px'>Bad</th>"
                        "<th style='width: 80px'>Not found</th>"
                        "<th style='width: 80px'>Redirect</th>"
                    "</tr>");
                ip_db->list(0, 100, ip_list_callback, &context);
                context.fp->sprintf(false, "</table><br/>Всего %d адресов<br/><br/>", context.counter);
            }
#endif
        }

        context.fp->sprintf(true, "</body></html>");
        context.fp->close_on_end();
    }

    static const char* generate_sessions_page(https_session_t* session)
    {
        http_request_t* request = &session->request;
        http_response_t* response = &session->response_holder;
        std::stringstream my_ss(std::stringstream::out);

        my_ss <<
            "<html lang='utf8'>"
            << admin_style << std::endl <<
            "<body>"
            "<table align='left' width: 100%; style='padding-right: 15px; margin-right: 10px; '>"
            "<tr>"
            "<th style='width: 30%;'>Remote</th>"
            "<th style='width: 40%;'>ID</th>"
            "<th style='width: 30%;'>Time</th>"
            "</tr>";

        session_mgr_t* sessions = get_active_sessions();
        for (auto const& ip : sessions->active_sessions) {
            for (auto const& session : *ip.second) {
                char str[155];
                session.second->get_transport()->describe(str, 155);
                char session_start[20];
                strftime(session_start, 20, "%Y-%m-%d %H:%M:%S", localtime(&session.second->start_time));
                my_ss <<
                    "<tr>"
                    "<td style='width: 30%; '>" << str << "</td>"
                    "<td style='width: 40%; '><span id='lid'>" << request->_cookies["lid"] << "</span></td>"
                    "<td style='width: 30%; '>" << session_start << "</td>"
                    "</tr>";
            }
        }
        my_ss << 
            "</table>"
            "</body>"
            "</html>";

        response->_body_size = (int) my_ss.str().size();
        std::string str = my_ss.str();
        response->_body = (char*) str.c_str();
        response->_content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, 200, response->_body_size);
        session->get_transport()->send(response->_header, response->_header_size);
        session->get_transport()->send(response->_body, response->_body_size);
        response->_body = nullptr;
        return nullptr;
    }


    void admin_action::process_req(https_session_t* session)
    {
        url_t           * url = &session->request.url;

        printf("HTTP-%s: Path:%s\n", http_method(url->method), url->pagename);

        switch (hash(url->pagename))
        {
        case hash("/admin/sessions"):
            generate_sessions_page(session);
            break;
        case hash("/admin/peers"):
            generate_ipv4_list(session);
            break;
        case hash("/admin/references"):
            generate_references_page(session);
            break;
        case hash("/admin/admin_header.html"):
            response_send_file(session, "pages/admin/admin_header.html");
            break;
        default:
            response_send_file(session, "pages/admin/default.html");
            break;
        }
    }
}