#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include "record.h"

const int MAX_RECORDS = 256;
const int STRING_MAX = 255;

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

    int field1=-1, field3=-1; //field2 is the returned contents of read
    sscanf(line, "%i, %*[^,], %i", &field1, &field3);   //discards field2

    struct read_params *params = malloc(sizeof(struct read_params));
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
    printf("close: %d\n", params->fd);
    close(params->fd);
}

void do_openat(struct record entry) {
    struct openat_params *params = (struct openat_params *) entry.params;
    printf("openat: %d %s %d %d\n", params->dirfd, params->pathname, params->flags, params->mode);
    openat(params->dirfd, params->pathname, params->flags, params->mode);
}

void do_read(struct record entry) {
    struct read_params *params = (struct read_params *) entry.params;
    printf("read: %d %zu\n", params->fd, params->count);
    void *buf = malloc(params->count);
    read(params->fd, buf, params->count);
    free(buf);
}

void do_pread64(struct record entry) {
    struct pread64_params *params = (struct pread64_params *) entry.params;
    printf("pread64: %d %zu %ld\n", params->fd, params->count, params->offset);
    void *buf = malloc(params->count);
    pread(params->fd, buf, params->count, params->offset);
    free(buf);
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
        case pread64_t:
            do_pread64(entry);
            break;
    }
}

void replay(struct record *records, int size) {
    for (int i = 0; i < size; i++) {
        double delta = records[i].time_delta * 1000 * 1000;   //milliseconds to nanoseconds
        printf("sleeping for %d\n", (int)delta);
        struct timespec length = {
                .tv_sec = 0,
                .tv_nsec = delta
        };
        nanosleep(&length, NULL);

        dispatch(records[i]);
    }
}

int main() {
    struct record records[256];
    int current_record = 0;

    FILE *fptr;
    fptr = fopen("trace1", "r");
    char buf[1024];
    while (fgets(buf, 1024, fptr) && current_record < MAX_RECORDS) {
        double delta;
        char call[16];
        int offset;
        int r = sscanf(buf, " %lf %n%15[a-z1-9]s", &delta, &offset, call);
        if (r != 2) {
            break;  //error parsing
        }
        printf("%lf %s %d\n", delta, call, offset);

        records[current_record].time_delta = delta;

        if (strcmp(call, "openat") == 0) {
            records[current_record].call = openat_t;
            new_openat(buf, offset + strlen("openat") + 1, &records[current_record]);   //skips past '('
        } else if (strcmp(call, "close") == 0) {
            records[current_record].call = close_t;
            new_close(buf, offset + strlen("close") + 1, &records[current_record]);
        } else if (strcmp(call, "read") == 0) {
            records[current_record].call = read_t;
            new_read(buf, offset + strlen("read") + 1, &records[current_record]);
        } else if (strcmp(call, "pread64") == 0) {
            records[current_record].call = pread64_t;
            new_pread64(buf, offset + strlen("pread64") + 1, &records[current_record]);
        } else {
            printf("NOT IMPLEMENTED\n");
            exit(-1);
        }

        current_record++;
    }

    fclose(fptr);

    replay(records, current_record);

    return 0;
}
