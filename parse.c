#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "record.h"

const int MAX_RECORDS = 256;

void new_openat(char *line, int offset, struct record *e) {
    line += offset;
    printf("%s", line);

    long field1, field2, field3, field4;
    sscanf(line, "%lx, %lx, %lx, %lx", &field1, &field2, &field3, &field4);

    struct openat_params *params = malloc(sizeof(struct openat_params));
    params->dirfd=field1;
    params->pathname= (char *) field2;
    params->flags=field3;
    params->mode=field4;

    e->params = params;
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
        sscanf(buf, " %lf %n%15[a-z]s", &delta, &offset, call);
        printf("%lf %s %d\n", delta, call, offset);

        records[current_record].time_delta = delta;

        if (strcmp(call, "openat") == 0) {
            records[current_record].call = openat_t;
            new_openat(buf, offset + strlen("openat") + 1, &records[current_record]);   //skips past '('
        }

        current_record++;
    }

    fclose(fptr);
    return 0;
}
