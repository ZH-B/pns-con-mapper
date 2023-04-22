#include <sys/time.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <inttypes.h>
#include <cstring>

#include "utils.h"

#define SHELL "/system/bin/sh"
#define MAX_OUTPUT_LENGTH 4096

uint64_t util_get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

char* sh_res(const char* command) {
    char* result = (char*) malloc(MAX_OUTPUT_LENGTH);
    if (!result) {
        return NULL;
    }
    memset(result, 0, MAX_OUTPUT_LENGTH);

    FILE* fp = popen(command, "r");
    if (!fp) {
        free(result);
        return NULL;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        strncat(result, buf, MAX_OUTPUT_LENGTH - strlen(result) - 1);
    }

    pclose(fp);
    return result;
}