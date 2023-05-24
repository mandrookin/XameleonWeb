#include <iostream>
#include <fstream>
#include <string>

#include "../json_stream.h"

using namespace std;

//#define AUTOTEST

namespace xameleon
{
    const char* json_stream_reader::parse_char(char ch)
    {
        if (state != string_state && (ch == '\n' || isspace(ch)))
            return nullptr;

        switch (state)
        {
        case starting:
            if (ch == '[')
            {
                state = array_state;
                push(wait_array_delimiter);
                Event_KeyValue(name, value, ARRAY, false);
            }
            else if (ch == '{')
            {
                state = object_state;
                push(wait_object_delimiter);
                Event_KeyValue(name, value, STRUCT, false);
            }
            else return "Bad json err 1";
            break;

        case array_state:
            if (ch == '{') {
                push(wait_object_delimiter);
                Event_KeyValue(name, value, STRUCT, false);
                name.clear();
                text.clear();
                state = object_state;
            }
            else if (ch == '\"') {
                push(wait_array_delimiter);
                value_type = STRING;
                name.clear();
                state = string_state;
                text.clear(); // = ch;
            }
            else if (ch == ']') {
                state = pop();
                name.clear();
                value.clear();
                Event_KeyValue(name, value, ARRAY, true);
            }
            else if (ch == '[') {
                cout << "Array in array\n";
                push(wait_array_delimiter);
                Event_KeyValue(name, value, ARRAY, false);
            }
            else if (ch == '-' || isdigit(ch)) {
                text = ch;
                state = number_state;
                value_type = NUMBER;
            }
            else
                return "Bad json err 2";
            break;

        case string_state:
            if (ch == '\\') {
                state = screen_state;
            }
            else if (ch == '\"') {
                state = pop();
            }
            else
                text += ch;
            break;

        case screen_state:
            if (wait_digits_count > 0) {
                if (wait_digits_count <= 4)
                {
                    hex16[4 - wait_digits_count--] = ch;
                }
                if (wait_digits_count == 0)
                {
                    hex16[4] = 0;
                    int ch16 = stoi((const char*)hex16, 0, 16);
                    if (ch16 < 0x80)
                        text += (char)ch16;
                    else if (ch16 < 0x800) {
                        text += (0xc0 + (ch16 >> 6));
                        text += (0x80 + (ch16 & 0x3f));
                    }
                    else {
                        text += (0xe0 + (ch16 >> 12));
                        text += (0x80 + ((ch16 >> 6) & 0x3f));
                        text += (0x80 + (ch16 & 0x3f));
                    }
                    state = string_state;
                }
                return nullptr;
            }
            state = string_state;
            switch (ch)
            {
            case '"':
            case '\\':
            case '/':
                break;
            case 'r':
                ch = '\r';
                break;
            case 'n':
                ch = '\n';
                break;
            case 't':
                ch = '\t';
                break;
            case 'b':
                ch = '\b';
                break;
            case 'f':
                ch = '\f';
                break;
            case 'u':
                state = screen_state;
                wait_digits_count = 4;
                return 0;
            default:
                return "Unparsed control sequence";
            }
            text += ch;
            break;

        case wait_array_delimiter:
            switch (ch) {
            case ',':
                value = text;
                if (!name.empty())
                    cout << ",\n";  // array item
                else if (!value.empty()) {
                    Event_KeyValue(name, value, value_type, false);
                    value.clear();
                }
                else
                    cout << ",\n";  // array item
                state = array_state;
                break;
            case ']':
                value = text;
                if (name.empty() && !value.empty()) {
                    Event_KeyValue(name, value, value_type, true);
                    value.clear();
                    text.clear();
                }
                Event_KeyValue(name, value, ARRAY, true);
                state = pop();
                break;

            case ':':
                name = text;
                state = enter_value;
                break;

            case '}':
                //                name = text;
                state = pop();
                value.clear();
                Event_KeyValue(name, value, STRUCT, true);
                break;

            default:
                return "Bad array delimiter";
            }
            break;

        case object_state:
            if (ch == '\"') {
                push(wait_object_delimiter); // Вижу на стеке уже это состояние
                state = string_state;
                text.clear(); // = ch;
                break;
            }
            if (ch == '{') {
                push(wait_object_delimiter);
                Event_KeyValue(name, value, STRUCT, false);
                break;
            }
            if (ch == '}') {
                name.clear();
                Event_KeyValue(name, value, STRUCT, true);
                state = pop();
                break;
            }
            throw "Object state recived unparsed character";
            break;

        case wait_object_delimiter:
            switch (ch)
            {
            case ',':
                value = text;
                if (name.length() != 0)
                    Event_KeyValue(name, value, value_type, false);
                else
                    cout << ",\n";
                name.erase();
                value.erase();
                state = object_state;
                break;
            case '}':
                value = text;
                if (name.length() != 0)
                    Event_KeyValue(name, value, value_type, true);
                name.erase();
                value.erase();
                text.erase();
                Event_KeyValue(name, value, STRUCT, true);
                state = pop();
                break;
            case ':':
                name = text;
                //                push(wait_object_delimiter);
                state = enter_value;
                break;
            case ']':
                Event_KeyValue(name, value, ARRAY, true);
                state = pop();
                break;
            default:
                return "Unparsed delimiter";
            }
            break;

        case enter_value:
            if (ch == '{') {
                push(wait_object_delimiter /*state*/);
                state = object_state;;
                Event_KeyValue(name, value, STRUCT, false);
                break;
            }
            if (ch == '[') {
                push(wait_array_delimiter);
                state = array_state;
                Event_KeyValue(name, value, ARRAY, false);
                name.clear();
                value.clear();
                break;
            }
            state = parse_value;
            // NO BREAK HERE!
        case parse_value:
            if (ch == '\"') {
                push(wait_object_delimiter);  // Проверь тут
                state = string_state;
                text.clear();
                value_type = STRING;
                break;
            }
            if (isdigit(ch) || ch == '-') {
                text = ch;
                state = number_state;
                value_type = NUMBER;
                break;
            }
            text = ch;
            state = lexem_state;
            value_type = NUL;
            return nullptr;

        case lexem_state:
            switch (ch)
            {
            case '}':
            case ']':
            case ',':
                state = (ch == ',') ? parse_value : pop();
                value_type = check_values(text);

                value = text;
                if (name.length() != 0)
                    Event_KeyValue(name, value, value_type,/*false*/ ch != ',');
                else
                    cout << ",\n";
                name.erase();
                value.erase();
                if (ch != ',')
                    Event_KeyValue(name, value, ch == '}' ? STRUCT : ARRAY, true);
                break;

            default:
                text += ch;
            }
            break;

        case number_state:
            if (ch == ',')
            {
                value = text;
 //               if (name.length() != 0)
                    Event_KeyValue(name, value, value_type, false);
 //               else
 //                   cout << ',' << endl;
                name = "";
                value = "";
                text.clear();
                state = enter_value;
                break;
            }
            if (ch == '}')
            {
                value = text;
                Event_KeyValue(name, value, value_type, true);
                name.erase();
                value.erase();
                text.erase();
                Event_KeyValue(name, value, STRUCT, true);
                state = pop();// wait_object_delimiter;
                break;
            }
            if (ch == ']')
            {
//                cout << ']' << endl;
                value = text;
                Event_KeyValue(name, value, value_type, true);
                name.erase();
                value.erase();
                text.erase();
                Event_KeyValue(name, value, ARRAY, true);
                state = pop(); // wait_array_delimiter;
                break;
            }
            text += ch;
            break;

        default:
            throw "Unparsed JSON state";
        }
        return nullptr;
    }

}


#ifdef AUTOTEST

namespace xameleon {

    class json_formatter : public json_stream_reader
    {
        int pos = 0;
        void    Event_KeyValue(string& name, string& value, types_t type, bool last)
        {
            cout << "\033[36m";

            if (!last || (type != ARRAY && type != STRUCT))
                for (int i = 0; i < pos; i++)
                    cout << "  ";

            if (!name.empty())
                cout << "\"" << name << "\": ";

            switch (type) {
            case ARRAY:
                if (!last) {
                    cout << "[\n";
                    pos++;
                }
                break;

            case  STRUCT:
                if (!last) {
                    cout << "{\n";
                    pos++;
                }
                break;

            case STRING:
                cout << "\"" << value << "\"" << (last ? "" : ",\n");
                break;

            case NUMBER:
                cout << value << (last ? "\n" : ",\n");
                break;
            case TRUE:
                cout << "true" << (last ? "\n" : ",\n");
                break;
            case FALSE:
                cout << "false" << (last ? "\n" : ",\n");
                break;
            case NUL:
                cout << "null" << (last ? "\n" : ",\n");
                break;
            default:
                throw "Unknown JSON FSM state";
            }

            if (last)
            {
#if false
                cout << (type != ARRAY ? "}\n" : "]\n");
#else
                switch (type) {
                case ARRAY:
                    pos--;
                    for (int i = 0; i < pos; i++)
                        cout << "  ";
                    cout << "]\n";
                    break;
                case STRUCT:
                    pos--;
                    for (int i = 0; i < pos; i++)
                        cout << "  ";
                    cout << "}\n";
                    break;
                default:
                    cout << "\n";
                    break;
                }
#endif
            }

            //    if (type != ARRAY)
            //        cout << "";
            //}

            cout << "\033[0m";
        }

    public:
        json_formatter() : json_stream_reader()
        {
        }
    };

}


#ifdef _WIN32
  extern "C" int SetConsoleCP( unsigned int wCodePageID);
  extern "C" int SetConsoleOutputCP( unsigned int wCodePageID);
#endif

int main(int argc, char* argv[])
{
    const char* json_filename = "json1.json";
    if (argc == 2)
        json_filename = argv[1];

    std::ifstream json_stream(json_filename);

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001); // CP_UTF8);
#else
    std::setlocale(LC_ALL, "en_US.UTF-8");
#endif

    try {

        xameleon::json_formatter      json_reader;

        if (json_stream.is_open()) {
            char ch;


            while (json_stream.get(ch)) {
                const char* msg = json_reader.parse_char(ch);
                if (msg) {
                    std::cerr << "File: " << json_filename << " - " << msg << std::endl;
                    break;
                };
            }
        }
        else
            cerr << "Unable open file " << json_filename << endl;
    }
    catch (const char* exception_message)
    {
        std::cerr << exception_message;
    }
    return 0;
}

#endif