#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG(fmt, ...)                                                          \
    do {                                                                       \
        time_t now = time(NULL);                                               \
        char time_str[20];                                                     \
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",              \
                 localtime(&now));                                             \
        printf("[%s %s:%d] " fmt "\n", time_str, __FUNCTION__, __LINE__,       \
               ##__VA_ARGS__);                                                 \
    } while (0)

uint64_t util_get_timestamp();

int sh(char *cmdstr);

int sh_aync(char *cmdstr);

char* sh_res(const char* command);