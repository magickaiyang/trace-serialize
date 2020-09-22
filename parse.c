#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include "record.h"

const int MAX_RECORDS = 2048;
const int STRING_MAX = 255;
void *buf;

void new_openat(char *line, int offset, struct record *e) {
    line += offset;

    int field1, field3, field4 = 0; //openat may only have 3 fields
    char *field2 = malloc(STRING_MAX + 1);
    sscanf(line, "%i, \"%[^\"]\", %i, %i", &field1, field2, &field3, &field4);

    struct openat_params *params = malloc(sizeof(struct openat_params));
    params->dirfd = field1;
    params->pathname = (char *) field2;
    params->flags = field3;
    params->mode = field4;

    e->params = params;
}

void new_read(char *line, int offset, struct record *e) {
    line += offset;

    int field1 = -1, field3 = -1; //field2 is the returned contents of read
    sscanf(line, "%i, %*[^,], %i", &field1, &field3);   //discards field2

    struct read_params *params = malloc(sizeof(struct read_params));
    params->fd = field1;
    params->count = field3;

    e->params = params;
}

void new_write(char *line, int offset, struct record *e) {
    line += offset;

    int field1 = -1, field3 = -1; //field2 is the returned contents of read
    sscanf(line, "%i, %*[^,], %i", &field1, &field3);   //discards field2

    struct write_params *params = malloc(sizeof(struct write_params));
    params->fd = field1;
    params->count = field3;

    e->params = params;
}

void new_pread64(char *line, int offset, struct record *e) {
    line += offset;

    int field1, field3, field4; //field2 is the returned contents
    sscanf(line, "%i, %*[^,], %i, %i", &field1, &field3, &field4);   //discards field2

    struct pread64_params *params = malloc(sizeof(struct pread64_params));
    params->fd = field1;
    params->count = field3;
    params->offset = field4;

    e->params = params;
}

void new_pwrite64(char *line, int offset, struct record *e) {
    line += offset;

    int field1, field3, field4; //field2 is the returned contents
    sscanf(line, "%i, %*[^,], %i, %i", &field1, &field3, &field4);   //discards field2

    struct pwrite64_params *params = malloc(sizeof(struct pwrite64_params));
    params->fd = field1;
    params->count = field3;
    params->offset = field4;

    e->params = params;
}

void new_close(char *line, int offset, struct record *e) {
    line += offset;

    int field1;
    sscanf(line, "%i", &field1);

    struct close_params *params = malloc(sizeof(struct close_params));
    params->fd = field1;

    e->params = params;
}

void do_close(struct record entry) {
    struct close_params *params = (struct close_params *) entry.params;
//    printf("close: %d\n", params->fd);
    close(params->fd);
}

void do_openat(struct record entry) {
    struct openat_params *params = (struct openat_params *) entry.params;
//    printf("openat: %d %s %d %d\n", params->dirfd, params->pathname, params->flags, params->mode);
    openat(params->dirfd, params->pathname, params->flags, params->mode);
}

void do_read(struct record entry) {
    struct read_params *params = (struct read_params *) entry.params;
//    printf("read: %d %zu\n", params->fd, params->count);
    read(params->fd, buf, params->count);
}

void do_write(struct record entry) {
    struct write_params *params = (struct write_params *) entry.params;
//    printf("write: %d %zu\n", params->fd, params->count);
    write(params->fd, buf, params->count);
}

void do_pread64(struct record entry) {
    struct pread64_params *params = (struct pread64_params *) entry.params;
//    printf("pread64: %d %zu %ld\n", params->fd, params->count, params->offset);
    pread(params->fd, buf, params->count, params->offset);
}

void do_pwrite64(struct record entry) {
    struct pwrite64_params *params = (struct pwrite64_params *) entry.params;
//    printf("pwrite64: %d %zu %ld\n", params->fd, params->count, params->offset);
    pwrite(params->fd, buf, params->count, params->offset);
}

void dispatch(struct record entry) {
    switch (entry.call) {
        case close_t:
            do_close(entry);
            break;
        case openat_t:
            do_openat(entry);
            break;
        case read_t:
            do_read(entry);
            break;
        case write_t:
            do_write(entry);
            break;
        case pread64_t:
            do_pread64(entry);
            break;
        case pwrite64_t:
            do_pwrite64(entry);
            break;
    }
}

void __attribute__((optimize("O0"))) replay(struct record *records, int size) { //prevents the loop from being optimized out
    for (int i = 0; i < size; i++) {
        double delta = records[i].time_delta; //seconds
        struct timespec ts1, ts2;
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        double duration = ts2.tv_sec + 1e-9 * ts2.tv_nsec
                          - (ts1.tv_sec + 1e-9 * ts1.tv_nsec);
        while (delta - duration > 20*1e-6) {    //less than 20 microseconds will be spinned. clock_gettime has an intrinsic latency of at least 20us
            clock_gettime(CLOCK_MONOTONIC, &ts2);
            duration = ts2.tv_sec + 1e-9 * ts2.tv_nsec
                       - (ts1.tv_sec + 1e-9 * ts1.tv_nsec);
        }
        long numSpin = (long)((delta-duration) * 140000000.0);  //empirical value
        for(long j=0;j<numSpin;j++);

        dispatch(records[i]);
    }
}

int main() {
    struct record records[MAX_RECORDS];
    int current_record = 0;

    FILE *fptr;
    fptr = fopen("trace1", "r");
    char linebuf[1024];
    while (fgets(linebuf, 1024, fptr) && current_record < MAX_RECORDS) {
        double delta;
        char call[16];
        int offset;
        int r = sscanf(linebuf, " %lf %n%15[a-z1-9]s", &delta, &offset, call);
        if (r != 2) {
            break;  //error parsing
        }

        records[current_record].time_delta = delta;

        if (strcmp(call, "openat") == 0) {
            records[current_record].call = openat_t;
            new_openat(linebuf, offset + strlen("openat") + 1, &records[current_record]);   //skips past '('
        } else if (strcmp(call, "close") == 0) {
            records[current_record].call = close_t;
            new_close(linebuf, offset + strlen("close") + 1, &records[current_record]);
        } else if (strcmp(call, "read") == 0) {
            records[current_record].call = read_t;
            new_read(linebuf, offset + strlen("read") + 1, &records[current_record]);
        } else if (strcmp(call, "write") == 0) {
            records[current_record].call = write_t;
            new_write(linebuf, offset + strlen("write") + 1, &records[current_record]);
        } else if (strcmp(call, "pread64") == 0) {
            records[current_record].call = pread64_t;
            new_pread64(linebuf, offset + strlen("pread64") + 1, &records[current_record]);
        } else if (strcmp(call, "pwrite64") == 0) {
            records[current_record].call = pwrite64_t;
            new_pwrite64(linebuf, offset + strlen("pwrite64") + 1, &records[current_record]);
        } else {
            printf("NOT IMPLEMENTED\n");
            return -1;
        }

        current_record++;
    }

    fclose(fptr);

    buf = malloc(5000);
    replay(records, current_record);

    return 0;
}
