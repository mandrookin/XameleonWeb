#include <random>

    static std::random_device              rd;
    static std::mt19937                    gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    inline char to_hex(int i)
    {
        return (char)(i > 9 ? 'a' + i - 9 : '0' + i);
    }

    void generate_uuid_v4(char * guid)
    {
        int i,j=0;

        for (i = 0; i < 8; i++) {
            guid[j++] = to_hex(dis(gen));
        }
        guid[j++] = '-';
        for (i = 0; i < 4; i++) {
            guid[j++] = to_hex(dis(gen));
        }
        guid[j++] = '-';
        guid[j++] = '4';
        for (i = 0; i < 3; i++) {
            guid[j++] = to_hex(dis(gen));
        }
        guid[j++] = '-';
        guid[j++] = to_hex(dis2(gen));
        for (i = 0; i < 3; i++) {
            guid[j++] = to_hex(dis(gen));
        }
        guid[j++] = '-';
        for (i = 0; i < 12; i++) {
            guid[j++] = to_hex(dis(gen));
        }
        guid[j] = 0;
    }
