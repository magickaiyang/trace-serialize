#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>

struct record {
    int fd;
    double delta;
    size_t offset;
    size_t length;
};

/*
0->snapshot_1.json	1->schema_1.json	2->cfmeta.json	3->segmeta.json	4->cf.dat
*/
int fdTable[8];

void openFiles() {
    fdTable[0] = open("lucy/snapshot_1.json", O_RDONLY);
    fdTable[1] = open("lucy/schema_1.json", O_RDONLY);
    fdTable[2] = open("lucy/cfmeta.json", O_RDONLY);
    fdTable[3] = open("lucy/segmeta.json", O_RDONLY);
    fdTable[4] = open("lucy/cf.dat", O_RDONLY);
}


const int MAX_RECORDS = 2048;
const int STRING_MAX = 255;
void *buf;

void do_pread64(struct record *e) {
    pread(e->fd, buf, e->length, e->offset);
}

void __attribute__((optimize("O0")))
replay(struct record *records, int size) { //prevents the loop from being optimized out
    for (int i = 0; i < size; i++) {
        double delta = records[i].delta; //seconds
        struct timespec ts1, ts2;
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        double duration = ts2.tv_sec + 1e-9 * ts2.tv_nsec
                          - (ts1.tv_sec + 1e-9 * ts1.tv_nsec);
        while (delta - duration > 20 *
                                  1e-6) {    //less than 20 microseconds will be spinned. clock_gettime has an intrinsic latency of at least 20us
            clock_gettime(CLOCK_MONOTONIC, &ts2);
            duration = ts2.tv_sec + 1e-9 * ts2.tv_nsec
                       - (ts1.tv_sec + 1e-9 * ts1.tv_nsec);
        }
        long numSpin = (long) ((delta - duration) * 140000000.0);  //empirical value
        for (long j = 0; j < numSpin; j++);

        do_pread64(&(records[i]));
    }
}

int endsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int main() {
    struct record records[MAX_RECORDS];
    int current_record = 0;
    buf = malloc(5000);
    double deltas[MAX_RECORDS];
    FILE *fptr;
    fptr = fopen("lucy/lucy1", "r");
    char linebuf[1024];
    openFiles();
    while (fgets(linebuf, 1024, fptr) && current_record < MAX_RECORDS) {
        double delta;
        size_t offset;
        size_t length;
        int r = sscanf(linebuf, "%lf %[^\t] %ld %ld", &delta, buf, &offset, &length);
        if (r != 4) {
            printf("Error parsing\n");
            return -1;  //error parsing
        }

        if (endsWith(buf, "snapshot_1.json")) {
            records[current_record].fd = fdTable[0];
        } else if (endsWith(buf, "schema_1.json")) {
            records[current_record].fd = fdTable[1];
        } else if (endsWith(buf, "cfmeta.json")) {
            records[current_record].fd = fdTable[2];
        } else if (endsWith(buf, "segmeta.json")) {
            records[current_record].fd = fdTable[3];
        } else if (endsWith(buf, "cf.dat")) {
            records[current_record].fd = fdTable[4];
        } else {
            printf("Error parsing\n");
            return -1;
        }

        deltas[current_record]=delta;
        if (current_record == 0) {
            records[current_record].delta = 0;
        } else {
            records[current_record].delta = delta - deltas[current_record-1];
        }

        records[current_record].length = length;
        records[current_record].offset = offset;

        printf("delta=%lf, fd=%d, offset=%ld, length=%ld\n", records[current_record].delta, records[current_record].fd,
               offset, length);
        current_record++;
    }
    fclose(fptr);

    replay(records, current_record);

    return 0;
}
