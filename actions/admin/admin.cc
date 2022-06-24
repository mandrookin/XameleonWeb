#include <string.h>
#include <sstream>
#include <sys/sysinfo.h>


#include "../../action.h"
#include "../../lib.h"
#include "../../session.h"
#include "../../session_mgr.h"

#include "../../db/ipv4_log.h"

// Полезное
// https://stackoverflow.com/questions/9975810/make-iframe-automatically-adjust-height-according-to-the-contents-without-using

namespace xameleon {

    extern session_mgr_t* get_active_sessions();

    static int ip_list_callback(void* obj, ipv4_record_t* ipv4)
    {
        FILE* f = (FILE *) obj;
        char first[20], last[20],buf[128];
        ipv4_log::sprint_ip(buf, ipv4->ip);
        strftime(first, 20, "%Y-%m-%d %H:%M:%S", localtime(&ipv4->first_seen));
        strftime(last, 20, "%Y-%m-%d %H:%M:%S", localtime(&ipv4->last_seen));

        fprintf(f, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>\n", 
            buf, first, last, 
            ipv4->counters.total_request,
            ipv4->counters.bad_request,
            ipv4->counters.not_found,
            ipv4->counters.http2https);
        return 0;
    }

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

    // Надо бы больше диагностики сюда добавить.
    static char* const do_list_ipv4(http_request_t* request, int* response_size)
    {
        int sz = 0;
        struct sysinfo      info;
        char* data = nullptr;

        FILE* fp_html = fopen("/tmp/debug_https_ip.html", "w+t");
        if (fp_html) 
        {
            int status = sysinfo(&info);
            if (status == 0) {
                char  server_period[128];
                char  service_period[128];
                print_uptime(server_period, info.uptime);
                print_uptime(service_period, xameleon::uptime());
                fprintf(fp_html, 
                    "<html lang='utf8'>"
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
                    "</style>"
                    "<body>"
                    "<p>System uptime: %s<br/>HTTPS service uptime: %s<br/></p>",
                    server_period, service_period);
            }

            xameleon::ipv4_log* ip_db = get_ip_database();
            if (ip_db) {
                fprintf(fp_html, 
                    "<h3>Статистика IP адресов с момента старта сервера</h3><br/>"
                    "<table><tr>"
                        "<th style='width: 160px'>IP</th>"
                        "<th style='width: 160px'>First seen</th>"
                        "<th style='width: 160px'>Last seen</th>"
                        "<th style='width: 80px'>Total</th>"
                        "<th style='width: 80px'>Bad</th>"
                        "<th style='width: 80px'>Not found</th>"
                        "<th style='width: 80px'>Redirect</th>"
                    "</tr>");
                ip_db->list(0, 100, ip_list_callback, fp_html);
                fprintf(fp_html, "</table>");
            }
            fprintf(fp_html, "</body></html>");

            sz = (int) ftell(fp_html);
            data = new char[sz];
            fflush(fp_html);
            fseek(fp_html, 0, SEEK_SET);
            if(fread(data, 1, sz, fp_html) != (size_t) sz)
                fprintf(stderr, "IPlist read error\n");
            *response_size = sz; // snprintf(list, 512, "Not ready yet\n");
            fclose(fp_html);
        }
        else
        {
            perror("Unable create temporary file\n");
        }

        return data;
    }

    static char* const do_sessions(http_request_t* request, int* response_size)
    {
        std::stringstream my_ss(std::stringstream::out);

        my_ss <<
            "<table align='left' width: 100%; style='padding-right: 15px; margin-right: 20px; '>"
            "<tr align='left'>"
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
                    // str << "  <- " << request->_cookies["lid"] << "<-" << request->_cache_control << "<br>\n";
                    "<tr>"
                    "<td style='width: 30%; '>" << str << "</td>"
                    "<td style='width: 40%; '><span id='lid'>" << request->_cookies["lid"] << "</span></td>"
                    "<td style='width: 30%; '>" << session_start << "</td>"
                    "</tr>";
            }
        }
        my_ss << "</table>";

        *response_size = my_ss.str().size();
        char* p = new char[*response_size + 1];
        strcpy(p, my_ss.str().c_str());
        return (char*)p;
    }

    http_response_t* admin_action::process_req(https_session_t* session, url_t* url)
    {
        http_request_t* request = &session->request;
        http_response_t* response = &session->response_holder;
        const char* req_name;
        switch (url->method) {
        case POST:
            req_name = "POST";
            break;
        case GET:
            req_name = "GET";
            break;
        case PUT:
            req_name = "PUT";
            break;
        case PATCH:
            req_name = "PATCH";
            break;
        case DEL:
            req_name = "DEL";
            break;
        default:
            req_name = "not-parsed-";
            break;
        }
        printf("HTTP-%s: Path:%s\n", req_name, url->rest);

        switch (hash(url->rest))
        {
        case hash("sessions"):
            response->_body = do_sessions(request, &response->_body_size);
            break;
        case hash("peers"):
            response->_body = do_list_ipv4(request, &response->_body_size);
            break;
        case hash("admin_header.html"):
            response->_body = alloc_file("pages/admin/admin_header.html", &response->_body_size);
            break;
        default:
            response->_body = alloc_file("pages/admin/default.html", &response->_body_size);
            break;
        }

        response->_code = 200;
        response->content_type = "text/html; charset=utf-8";
        response->_header_size = response->prepare_header(response->_header, response->_code, response->_body_size);

        return response;
    }
}