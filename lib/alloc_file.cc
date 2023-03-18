#define _CRT_SECURE_NO_WARNINGS 1
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define open _open
#define read _read
#define close _close
#endif
#endif

char* alloc_file(const char* filename, int* size)
{
    struct stat path_stat;
    int status = stat(filename, &path_stat);

    if (status < 0 || !S_ISREG(path_stat.st_mode)) {
        fprintf(stderr, "Only regular files can be send. Wrong routes settings.\n");
        *size = 0;
        return nullptr;
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Unable open file: %s\n", filename);
        return nullptr;
    }

#if true
    * size = path_stat.st_size;
#else
    * size = (int)lseek(fd, 0, SEEK_END);
    if (*size < 0) {
        perror("Unable lseek");
    }
    lseek(fd, 0, SEEK_SET);
#endif
    char* data = new char[*size];
    if (data != nullptr) {
        *size = read(fd, data, *size);
    }
    else {
        //free(data);
        data = nullptr;
    }
    close(fd);
    return data;
}

