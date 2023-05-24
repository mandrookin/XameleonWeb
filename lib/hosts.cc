#include <fstream>

#include "../hosts.h"
#include "../json_stream.h"
#include "../http.h"

const char* config_filename = "servconfig.json";

namespace xameleon
{
    static hostmap_t	hostmap;

    host_t* find_host(const char* host, int port)
    {
        std::string key(host);
        key += ':';
        key += std::to_string(port);

        //     std::string key << host << ':' << std::to_string(port);

        return &hostmap[key];
    }

    host_t* find_host(const char* key)
    {
        host_t* host = nullptr;
        std::string host_key(key);
        if (host_key.find(':') == std::string::npos)
        {
            host_key += ":80";
        }
        if (hostmap.find(host_key) == hostmap.end())
            return nullptr;
        host = &hostmap[host_key];
        host->access_count++;
        return host;
    }

    class config : public json_stream_reader {
        
        typedef enum {
            entry_json,
            values,
            hosts,
            host
        } state_t;

        state_t state;
        int http_session_timeout;
        int https_session_timeout;
        ////host_t  site;
        std::string     name;
        std::string     home;
        mode_t         mode;


    public:
        config();
        void Event_KeyValue(std::string& name, std::string& value, types_t type, bool last);
    };

    static config init;

    void config::Event_KeyValue(std::string& key, std::string& value, types_t type, bool last)
    {
        switch (state)
        {
            case entry_json:
                if (type != types_t::STRUCT)
                    throw "Bad format of servconf.json";
                state = values;
                break;
            case values:
                switch (hash(key.c_str()))
                {
                case hash("http_timeout"):
                    http_session_timeout = std::atoi(value.c_str());
                    break;
                case hash("https_timeout"):
                    https_session_timeout = std::atoi(value.c_str());
                    break;
                case hash("hosts"):
                    if (type != types_t::ARRAY)
                        throw "json hosts format error";
                    state = hosts;
                    break;
                default:
                    throw "Config unknown identitcator";
                }
                break;
            case hosts:
                if (last == true && type == types_t::ARRAY)
                    state = values;
                else if(type != types_t::STRUCT)
                    throw "sevrconfig.json host array format error";
                state = host;
                break;
            case host:
                if (last == true && type == types_t::STRUCT) {
                    std::string key = this->name + ":80";
                    host_t* h = &hostmap[key.c_str()];
                    h->port = 80;
                    h->hostname = this->name;
                    h->host_and_port = key;
                    h->home = home;
                    h->mode = mode;
                    h->http_session_maxtime = http_session_timeout;
                    state = hosts;
                }
                else switch (hash(key.c_str()))
                {
                case hash("home"):
                    home = value;
                    break;
                case hash("proxy"):
                    home = value;
                    break;
                case hash("name"):
                    name = value;
                    break;
                case hash("mode"):
                    switch (hash(value.c_str()))
                    {
                    case hash("proxy"):
                        mode = proxy;
                        break;
                    case hash("site"):
                        mode = pages_and_cgi;
                        break;
                    case hash("default"):
                        mode = pages_and_cgi;
                        break;
                    default:
                        throw "Bad server mode";
                    }
                    break;
                default:
                    throw "sevrconfig.json unknown host option";
                }
                break;
        default:
            break;
        }
    }

    config::config()
    {
        std::ifstream json_config_stream(config_filename);

        if (!json_config_stream)
            puts("Dont believe some sites");

        state = entry_json;

        if ( !json_config_stream.is_open()) {
            puts("\033[36mUnable open servconfig.json, server running in demo mode.\033[0m\n");

            host_t* h = &hostmap["127.0.0.1"];
            h->port = 80;
            h->hostname = "localhost";
            h->host_and_port = "localhost:80";
            h->home = "pages";
            return;
        }

        char ch;
        try {
            while (json_config_stream.get(ch)) {
                const char* msg = parse_char(ch);
                if (msg) {
                    fprintf(stderr, "Error reading config: %s\n", msg);
                    break;
                };
            }
        }
        catch (const char * error)
        {
            fprintf(stderr, "JSON stream reader: %s\n", error);
            exit(3);
        }
    
    }

}
