#include <string>

namespace xameleon {
 
    class json_stream_reader {

        typedef enum {
            starting,
            array_state,
            wait_array_delimiter,
            object_state,
            wait_object_delimiter,
            string_state,
            screen_state,
            number_state,
            lexem_state,
            enter_value,
            parse_value,
            finish
        } states_t;

    protected:
        typedef enum {
            ERR,
            STRING,
            NUMBER,
            TRUE,
            FALSE,
            NUL,
            ARRAY,
            STRUCT
        } types_t;

    private:
        states_t        state = starting;
        unsigned int    wait_digits_count = 0;
        types_t         value_type = ERR;

        std::string     name;
        std::string     value;
        std::string     text;

        int             stack_ptr = 0;
        states_t        stack[22];
        unsigned char   hex16[5];

    private:

        void push(states_t state)
        {
            if (stack_ptr >= 0 && stack_ptr < 21)
                stack[stack_ptr++] = state;
            else
                throw "States stack overflow";
        }

        states_t pop()
        {
            states_t state;
            if (--stack_ptr >= 0 && stack_ptr < 22) {
                state = stack[stack_ptr];
                stack[stack_ptr] = (states_t)-1;
                return state;
            }
            throw "States stack underflow";
        }

        types_t check_values(std::string& buff) {
            if (buff == "true")
                return TRUE;
            if (buff == "false")
                return FALSE;
            if (buff == "null")
                return NUL;
            return ERR;
        }

    protected:
        virtual void    Event_KeyValue(std::string& name, std::string& value, types_t type, bool last) = 0;

    public:
        const char* parse_char(char ch);

        json_stream_reader() {
            state = starting;
            push(finish);
        }
    };
}
