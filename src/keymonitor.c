#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "keymonitor.h"
#include "utils.h"

static pthread_t monitorthrd;

/*
#define ABS_MT_POSITION_X	0x35	 Center X touch position
#define ABS_MT_POSITION_Y	0x36	 Center Y touch position

#define BTN_TOUCH		0x14a

#define BTN_A			0x130
#define BTN_B			0x131
#define BTN_X			0x133
#define BTN_Y			0x134

#define BTN_TL2			0x138
#define BTN_TR2			0x139

#define BTN_THUMBL		0x13d
#define BTN_THUMBR		0x13e

*/

void (*key_listener)(struct input_event event) = NULL;

void set_key_listener(void (*listener)(struct input_event event)) { key_listener = listener; }

static int fd_event_source = 0;

static int flag_sig_keymonitor_stop = 0;

static struct group_key_stats_t group_key_stats;

void input_tap_event(int x, int y) {
    struct input_event e;

    e.code = ABS_MT_TRACKING_ID;
    e.type = EV_ABS;
    e.value = 1;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = ABS_MT_POSITION_X;
    e.type = EV_ABS;
    e.value = x;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = ABS_MT_POSITION_Y;
    e.type = EV_ABS;
    e.value = y;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = BTN_TOUCH;
    e.type = EV_KEY;
    e.value = 1;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = 0;
    e.type = EV_SYN;
    e.value = SYN_REPORT;
    write(fd_event_source, &e, sizeof(struct input_event));

    usleep(2 * 1000);
    struct timeval t;
    gettimeofday(&t, NULL);
    e.time = t;

    e.code = BTN_TOUCH;
    e.type = EV_KEY;
    e.value = 0;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = ABS_MT_TRACKING_ID;
    e.type = EV_ABS;
    e.value = -1;
    write(fd_event_source, &e, sizeof(struct input_event));

    e.code = 0;
    e.type = EV_SYN;
    e.value = SYN_REPORT;
    write(fd_event_source, &e, sizeof(struct input_event));
}

void *key_monitor_funtion(void *args) {
    struct input_event event;
    while (!flag_sig_keymonitor_stop) {
        memset(&event, 0, sizeof(event));
        int readed_size = read(fd_event_source, &event, sizeof(event));
        if (/*event.type != EV_SYN &&*/ readed_size == sizeof(event)) {
            // process Key event
            if (key_listener != NULL) {
                key_listener(event);
            }

            switch (event.code) {
                case BTN_TL2:
                    group_key_stats.LT_hold = event.value;
                    break;
                case BTN_THUMBL:
                    group_key_stats.LS_hold = event.value;
                    break;
            }
        }
    }
    close(fd_event_source);
    LOG("stop key monitorthrd");
    return NULL;
}

int start_key_monitor() {
    flag_sig_keymonitor_stop = 0;
    fd_event_source =  open(INPUT_SOURCE, O_RDWR);
    int ret = 1;

    if (fd_event_source > 0) {
        pthread_create(&monitorthrd, NULL, key_monitor_funtion, NULL);
        ret = 0;
    } else {
        LOG("event source gett failed %s\n", strerror(errno));
    }

    return ret;
}

void stop_key_monitor() {
    flag_sig_keymonitor_stop = 1;
}

struct group_key_stats_t *get_group_key_stats() {
    return &group_key_stats;
}
