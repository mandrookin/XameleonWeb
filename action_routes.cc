#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include <sstream>

#include "http.h"
#include "action.h"

class endpoint_t
{
public:
    std::string name;
    std::map<std::string, endpoint_t *> _child_points;

    http_action_t *  _get;
    http_action_t *  _post;
    http_action_t *  _put;
    http_action_t *  _patch;
    http_action_t *  _del;

    endpoint_t(std::string name)
    {
        this->name = name;
        _get = nullptr;
        _post = nullptr;
        _put = nullptr;
        _patch = nullptr;
        _del = nullptr;
    }
};

static endpoint_t   root("/");

static endpoint_t * add_segment(endpoint_t * node, std::string name)
{
    if (node->_child_points.count(name) != 0) {
        node = node->_child_points[name];
    }
    else {
        endpoint_t * endpoint = new endpoint_t(name);
        node->_child_points[name] = endpoint;
        node = endpoint;
    }
    return node;
}

int add_action_route(const char * path, http_method_t method, http_action_t * handler)
{
    if (*path != '/') {
        fprintf(stderr, "path must be started from root\n");
        return -1;
    }

    endpoint_t * node = &root;
    std::string segment;

    if (path[1] == '\0') {
        node = add_segment(node, "/");
    }

    for (int i = 1; path[i]; i++) {
        if (path[i] == '/') {
            if (segment.size() > 0) {
                node = add_segment(node, segment);
                if (path[i + 1] == '\0')
                    node = add_segment(node, "/");
                segment.clear();
            }
            continue;
        }
        segment += path[i];
    }
    if (segment.size() > 0) 
        node = add_segment(node, segment);

    http_action_t ** slot = nullptr;
    switch (method)
    {
    case POST:
        slot = &node->_post;
        break;
    case GET:
        slot = &node->_get;
        break;
    case PUT:
        slot = &node->_put;
        break;
    case PATCH:
        slot = &node->_patch;
        break;
    case DEL:
        slot = &node->_del;
        break;
    default:
        fprintf(stderr, "Not implemented HTTP method\n");
        return -2;
    }
    if (*slot != nullptr) {
        fprintf(stderr, "HTTP method already set: %s\n", path);
        return -3;
    }
    *slot = handler;
    return 0;
}

http_action_t * find_http_action(url_t &url)
{
    http_action_t * route = nullptr;

    endpoint_t * node = &root;
    std::string segment;
    std::vector<std::string> chunks;
    int positions_count = 0;
    int positions[32];
    int prev_pos = 0;
    int i;

    url.rest = nullptr;

    for (i = 1; url.path[i]; i++) {
        if (url.path[i] != '/') {
            segment += url.path[i];
            continue;
        }
        if (url.path[i + 1] == '\0') {
            chunks.push_back(segment);
            segment = '/';
            i++;
            break;
        }
        if (segment.size() == 0)
            continue;
        positions[positions_count++] = i;
        chunks.push_back(segment);
        segment.clear();
    }
    chunks.push_back(segment);
    segment.clear();
    url.rest = url.path + i;

    int sz = chunks.size();
    for (i = 0; i < sz; i++) {
        segment = chunks[i];
        if (node->_child_points.count(segment) == 0) {
            node = node->_child_points["/"];
            if ( node == nullptr)
            {
                // Полюбому ошибка
                url.rest = url.path + 1;
                return nullptr;
            }
//            url.path[prev_pos] = '\0';
            url.rest = url.path + prev_pos + 1;
            break;
        }
        else {
            node = node->_child_points[segment];
            prev_pos = positions[i];
        }
    }

    if (node) {
        switch (url.method)
        {
        case POST:
            route = node->_post;
            break;
        case GET:
            route = node->_get;
            break;
        case PUT:
            route = node->_put;
            break;
        case PATCH:
            route = node->_patch;
            break;
        case DEL:
            route = node->_del;
            break;
        default:
            fprintf(stderr, "Not implemented HTTP method\n");
        }
    }

    return route;
}

#if DEBUG_ACTIONS

#pragma region Test

#include <cstring>

void debug_travers(endpoint_t * node, int tabsc)
{
    for (int i = 0; i < tabsc; i++)
        printf("  ");
    printf("%s\n", node->name.c_str());
    tabsc += 1;
    for (auto const& it : node->_child_points) {
        debug_travers(it.second, tabsc);
    }
}

int main_test(int argc, char * argv[])
{
    http_action_t * fake = (http_action_t*)1;
     
//    add_action_route("/", GET, fake);
    add_action_route("/empty", GET, fake);
    add_action_route("/empty/", GET, fake);
    add_action_route("/double/one", GET, fake);
    add_action_route("/double/two", GET, fake);
    add_action_route("/long/too/far/away", GET, fake);
    add_action_route("/poops/two", GET, fake);
    add_action_route("/poops/two/jopech", GET, fake);
    add_action_route("/poops/two//jopech/hulla", GET, fake);
    add_action_route("/poops/second/bucks", GET, fake);
    add_action_route("/bump/", GET, fake);
    add_action_route("/poops/two/jopech/", GET, fake);

    debug_travers(&root, 1);

    std::vector<std::string> urls{ 
        "/Blue", 
        "/empty",
        "/empty/",
        "/empty/duck",
        "/poops/two",
        "/poops/two/",
        "/poops/two/three",
        "/poops/two/jopech",
        "/bump",
        "/bump/truck/them/all/everyone",
        "/bump/luck"
    };

//    std::string args;
    for (int i = 0; i < urls.size(); i++) {
        url_t url;
        url.method = GET;
        strncpy(url.path, urls[i].c_str(), sizeof(url.path));
        fake = find_http_action(url);
        printf("  %-30s %14s '%s'\n", urls[i].c_str(), fake ? "Found" : "Not found", url.rest);
    }

    return 0;
}
#pragma endregion

#endif

