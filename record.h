#ifndef TRACE_RECORD_H
#define TRACE_RECORD_H

#include <unistd.h>

enum call_type {openat_t, read_t, write_t, close_t, pread64_t, pwrite64_t};

struct record {
    double time_delta;
    enum call_type call;
    void *params;
};

struct openat_params {
    int dirfd;
    char *pathname;
    int flags;
    int mode;
};

struct read_params {
    int fd;
//    void *buf;
    size_t count;
};

struct write_params {
    int fd;
//    void *buf;
    size_t count;
};

struct close_params {
    int fd;
};

struct pread64_params {
    int fd;
//    void *buf;
    size_t count;
    off_t offset;
};

struct pwrite64_params {
    int fd;
//    void *buf;
    size_t count;
    off_t offset;
};

#endif //TRACE_RECORD_H
