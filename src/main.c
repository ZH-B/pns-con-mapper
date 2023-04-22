#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#include "config.h"
#include "utils.h"
#include "keymonitor.h"
#include "scranal.h"
#include "assit_ui.h"

int origin_sc_w = DEFAULT_ORIGIN_SC_W;
int origin_sc_h = DEFAULT_ORIGIN_SC_H;
int origin_sc_skill_slot_l = DEFAULT_ORIGIN_SC_SKILL_SLOT_L;
int origin_sc_skill_slot_r = DEFAULT_ORIGIN_SC_SKILL_SLOT_R;
int origin_sc_skill_slot_h = DEFAULT_ORIGIN_SC_SKILL_SLOT_H;
int origin_sc_item_spac = DEFAULT_ORIGIN_SC_SKILL_ITEM_SPAC;

static int flag_stop_sig = 0;

void key_event_process(struct input_event event) {
    LOG("key_event_process key: 0x%x, type: %d, value: %d ts: %lu",
           event.code, event.type, event.value,
           (event.time.tv_sec * 1000 + event.time.tv_usec / 1000));
    enum skill_t target = SKILL_T_INVALID;

    switch (event.code) {
    case BTN_A: // SKILL B
        target = SKILL_T_B;
        break;
    case BTN_B: // SKILL W
        target = SKILL_T_W;
        break;
    case BTN_X: // SKILL R
        target = SKILL_T_R;
        break;
    case BTN_Y: // SKILL Y
        target = SKILL_T_Y;
        break;
    default:
        break;
    }

    if (target != SKILL_T_INVALID) {
        LOG("key_event_process target %d", target);
        int s_group[SKILL_GROUP_SIZE_MAX] = {-1, -1, -1};
        int s_group_index = 0;
        int target_count = 0;
        enum skill_t *skill_slot = get_skill_slot();
        for (int i = SKILL_SLOT_SIZE - 1; i >= 0; i--) {
            if (skill_slot[i] != target) {
                target_count = 0;
            }
            if (skill_slot[i] == target) {
                target_count++;
                if (target_count > SKILL_GROUP_SIZE_MAX || target == SKILL_T_W) {
                    target_count = 1;
                }
                if (target_count == 1) {
                    for (int j = s_group_index; j < SKILL_GROUP_SIZE_MAX; j++) {
                        s_group[j] = i;
                    }
                    s_group_index++;
                }
            }
        }

        struct group_key_stats_t* p_group_key_stats = NULL;

        p_group_key_stats = get_group_key_stats();

        int g_index = 0;
        if (p_group_key_stats->LT_hold) {
            g_index = 1;
        }

        if (p_group_key_stats->LS_hold) {
            g_index = 2;
        }

        int tap_skill_index = s_group[g_index];

        LOG("key_event_process target %d, [%d, %d, %d]", target, s_group[0], s_group[1], s_group[2]);

        if (tap_skill_index >= 0) {
            int tap_x = origin_sc_skill_slot_l + tap_skill_index * origin_sc_item_spac;
            int tap_y = origin_sc_skill_slot_h;

            struct input_event output_event;

            memcpy(&output_event, &event, sizeof(struct input_event));

            input_tap_event(tap_x, tap_y);
        }
    }
}

void parse_screen_resolution(char* output, int* width, int* height) {
    char* p = strstr(output, "DisplayDeviceInfo");
    if (p != NULL) {
        p = strstr(p, "local");
        if (p != NULL) {
            p = strstr(p, " ");
            if (p != NULL) {
                *width = atoi(p);
                p = strstr(p, "x");
                if (p != NULL) {
                    *height = atoi(p + 1);
                }
            }
        }
    }
}

int get_touch_coordinates(int* x1, int* y1, int* x2, int* y2) {
    int fd = open(INPUT_SOURCE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    
    struct input_event ev;
    int count = 0;
    while (count < 4) {
        if (read(fd, &ev, sizeof(struct input_event)) < 0) {
            perror("read");
            close(fd);
            return -1;
        }
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X) {
                if (count == 0) {
                    *x1 = ev.value;
                } else if (count == 2) {
                    *x2 = ev.value;
                }
            } else if (ev.code == ABS_MT_POSITION_Y) {
                if (count == 1) {
                    *y1 = ev.value;
                } else if (count == 3) {
                    *y2 = ev.value;
                }
            }
            if((ev.code == ABS_MT_POSITION_X || ev.code == ABS_MT_POSITION_Y )&& ev.value > 0) {
                count++;
            }
        }
    }
    close(fd);
    return 0;
}

int save_params(InitParams *params) {
    FILE *fp = fopen(PARAMS_FILE, "wb");
    if (!fp) {
        printf("Error opening file %s\n", PARAMS_FILE);
        return -1;
    }
    fwrite(params, sizeof(InitParams), 1, fp);

    fclose(fp);
    return 0;
}

int load_params(InitParams *params) {
    FILE *fp = fopen(PARAMS_FILE, "rb");
    if (!fp) {
        printf("Error opening file %s\n", PARAMS_FILE);
        return -1;
    }

    // 从文件中读取参数
    fread(params, sizeof(InitParams), 1, fp);

    fclose(fp);
    return 0;
}

int calib_params() {
    int width = 0, height = 0;
    char* res = sh_res("dumpsys display");
    // 获取屏幕分辨率
    parse_screen_resolution(res, &width, &height);
    LOG("获得屏幕分辨率：%d x %d", width, height);

    // 根据接下来两次点击事件, 配置技能槽参数
    int x1 = 0, y1= 0, x2= 0, y2= 0;
    while (1) {
        LOG("请分别点击最左侧/右侧 技能球 确认技能槽位置");
        get_touch_coordinates(&x1, &y1, &x2, &y2);
        int length = x2 - x1;
        LOG("获得技能槽位置 (start x1, y1: %d, %d  x2, y2 %d, %d--- length: %d 输入Y 确认)", x1, y1, x2, y2, length);
        char input;
        scanf("%c", &input);

        if(input == 'Y' || input == 'y') {
            break;
        }
    }

    InitParams params = {
        width,height,

        x1,y1,x2
    };

    save_params(&params);

    return 0;
}

int init_params() {
    int err = 0;
    InitParams params;
    err = load_params(&params);

    origin_sc_w = params.sc_w;
    origin_sc_h = params.sc_h;

    origin_sc_skill_slot_l = params.sc_skill_slot_l;
    origin_sc_skill_slot_h = params.sc_skill_slot_h;
    origin_sc_skill_slot_r = params.sc_skill_slot_r;

    origin_sc_item_spac = (origin_sc_skill_slot_r - origin_sc_skill_slot_l) / (SKILL_SLOT_SIZE - 1);

    return err;
}

void handle_stop(int signo) {
    LOG("sig_int");
    flag_stop_sig = 1;
}

int main(int argc, char **argv) {

    int err = 0;
    LOG("main start");

    if (argc > 1 && strcmp(argv[1], "init") == 0) {
        err = calib_params();
    }

    if (access(PARAMS_FILE, R_OK)) {
        err = calib_params();
    }

    err = init_params();

    if (err) {
        return err;
    }

    start_key_monitor();
    start_scr_anal();
    set_key_listener(key_event_process);
    signal(SIGINT, handle_stop);

    start_ui_assit();
    while (!flag_stop_sig) {
        usleep(33 * 1000);
    }
    stop_ui_assit();
    stop_scr_anal();
    stop_key_monitor();
    
    LOG("main end");
    exit(0);
    return err;
}
