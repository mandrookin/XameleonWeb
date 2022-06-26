// https://stackoverflow.com/questions/32936646/getting-the-string-length-on-utf-8-in-c

inline size_t count_utf8_code_points(const char *s) {
    size_t count = 0;
    while (*s) {
        count += (*s++ & 0xC0) != 0x80;
    }
    return count;
}
