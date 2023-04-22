#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include "keymonitor.h"
#include "scranal.h"
#include "utils.h"
#include "config.h"

#define PKG_NAME "com.example.pnsconmapperassist"
#define SERVECE_NAME ".PNSUIService"

#define PIPE_FN PATH_ROOT "con_mapper_ui.fifo"
#define UI_MERGIN 5

static int ui_assit_running = 0;
static int fd_pipe = -1;

int start_ui_assit(){
    LOG("start_ui_assit");
    char* pm = sh_res("pm list packages");
    if (strstr(pm, PKG_NAME) == NULL) {
        LOG("can not found :" PKG_NAME " no ui assiter");
        return -1;
    }

    sh_res("am start-foreground-service -n " PKG_NAME "/" SERVECE_NAME " -e pip_name " PIPE_FN);

    ui_assit_running = 1;
    return 0;
}

int draw_rect(int x, int y, struct pix_color_t color, int opacity) {
    int ui_item_rect_size = origin_sc_item_spac / 2 + 2 * UI_MERGIN;
    int l = x - ui_item_rect_size / 2;
    int t = y - ui_item_rect_size / 2;
    int r = x + ui_item_rect_size / 2;
    int b = y + ui_item_rect_size / 2;
    
    char info[50];
    sprintf(info, "%d,%d,%d,%d,%d,%d,%d,%d\n", l, t, r, b, opacity,color.r, color.g, color.b);
    LOG("draw_rect %s", info);
    int s = write(fd_pipe, info, strlen(info));
    if (s == -1) {
        LOG("%d writed  %s", s, strerror(errno));
    }

    return 0;
}

int update_assit_ui(enum skill_t* skill_slot) {
    if (!ui_assit_running) {
        return -1;
    }
    if (fd_pipe == -1) {
        fd_pipe = open(PIPE_FN, O_WRONLY | O_NONBLOCK);
        if (fd_pipe == -1) {
            LOG("Open  %s,   %s", PIPE_FN, strerror(errno));
        }
    }

    LOG("update_assit_ui :");

    struct group_key_stats_t* p_group_key_stats = NULL;

    p_group_key_stats = get_group_key_stats();

    int target = 0;

    if (p_group_key_stats->LT_hold) {
        target = 1;
    }

    if (p_group_key_stats->LS_hold) {
        target = 2;
    }

    int mark_item[SKILL_SLOT_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0};

    int group_index_r = -1;
    int group_index_b = -1;
    int group_index_y = -1;
    int cur_index = -1;

    enum skill_t last_skill_item = SKILL_T_INVALID;
    
    for (int i = SKILL_SLOT_SIZE - 1; i >=0; i--) {
        if (skill_slot[i] == last_skill_item) {
            cur_index++;
            if (cur_index == SKILL_GROUP_SIZE_MAX) {
                cur_index = 0;
            }
        } else {
            cur_index = 0;
            last_skill_item = skill_slot[i];
        }
        int group_cur;
        if (cur_index == 0) {
            switch (skill_slot[i]) {
            case SKILL_T_R:
                group_index_r++;
                group_cur = group_index_r;
                break;
            case SKILL_T_B:
                group_index_b++;
                group_cur = group_index_b;
                break;
            case SKILL_T_Y:
                group_index_y++;
                group_cur = group_index_y;
                break;
            default:
                break;
            }
        }

        if (cur_index == 0) {
            for (int j = i; j >= 0; j--) {
                if (skill_slot[j] == skill_slot[i]) {
                    if (group_cur == target) {
                        mark_item[j] = 1;
                    } else {
                        mark_item[j] = 0;
                    }
                }
            }
        }
    }

    
    for (int i = 0; i < SKILL_SLOT_SIZE; i++) {
        if (skill_slot[i] != SKILL_T_INVALID) {
            struct pix_color_t color;
            switch (skill_slot[i]) {
                case SKILL_T_R:
                    color = STD_R;
                    break;
                case SKILL_T_Y:
                    color = STD_Y;
                    break;
                case SKILL_T_B:
                    color = STD_B;
                    break;
                case SKILL_T_W:
                    color = STD_W;
                    break;
                default:
                    break;
            }
            int opacity = 0x0f;
            if (mark_item[i]) {
                opacity = 0xff;
            }

            draw_rect(origin_sc_skill_slot_l + i * origin_sc_item_spac,
                      origin_sc_skill_slot_h, color, opacity);
        }
    }
    const char* sync_info = "sync\n";
    int s = write(fd_pipe, sync_info, strlen(sync_info));
    if (s == -1) {
        LOG("%d writed  %s", s, strerror(errno));
        fd_pipe = -1;
    }
    
    return 0;
}

int stop_ui_assit(){
    sh_res("am stopservice " PKG_NAME "/" SERVECE_NAME);
    fd_pipe = -1;
    ui_assit_running = 0;
    
    return 0;
}