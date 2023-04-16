#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "utils.h"
#include "config.h"
#include "scranal.h"

#define DUMP_FRAME_TOTAL 30
static int dump_frame_count = 0;

#define FIFO_FN PATH_ROOT "screenfb.fifo"

#define PIX_FMT_SIZ 3

#define JUDG_VAL_W 400

#define STAT_BORDER 2
#define DOWN_SCALE_RATE 2

#define MIN_COLOR_AVG_SCORE 10

int std_o[3];

int sample_sc_skill_slot_l = 0;
int sample_sc_skill_slot_r = 0;
int sample_sc_skill_slot_h = 0;

static int sample_sc_w = 0;
static int sample_sc_h = 0;
static int sample_buffer_size = 0;

int sample_sc_item_spac = 0;
int sample_sc_item_size = 0;

static pthread_t scrcap_thrd;
static pthread_t scranal_thrd;

static int stop_anal_flag = 0;
static int sc_cap_pid = -1;
static int fd_pipe_fifo = -1;
static void *sc_buffer = NULL;

int squa_euc_dist(struct pix_color_t p1, struct pix_color_t p2) {
    int r_dist = p1.r - p2.r;
    int g_dist = p1.g - p2.g;
    int b_dist = p1.b - p2.b;
    return r_dist * r_dist + g_dist * g_dist + b_dist * b_dist;
}

double euc_dist(struct pix_color_t p1, struct pix_color_t p2) {
    return sqrt(1.0 * squa_euc_dist(p1, p2));
}

// 用于标记检测结果
void set_pix_color(void *p_pix, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t color[] = {r, g, b};
    if (p_pix != NULL) {
        memcpy(p_pix, color, PIX_FMT_SIZ);
    }
}

int get_score_by_dist(int dist, int judg_val) {
    int score = (int)(100 * (1.0 - (1.0 * dist / judg_val)));
    if (score < 0)
        return 0;
    return score;
}

// 返回两个向量相对平均点的余弦, 增益绿色通道，增加颜色区分度
float cos_sim(struct pix_color_t p1, struct pix_color_t p2) {
    int vector_1[] = {
        (int)p1.r - std_o[0],
        ((int)p1.g - std_o[1]) * 2,
        (int)p1.b - std_o[2]
    };

    int vector_2[] = {
        (int)p2.r - std_o[0],
        ((int)p2.g - std_o[1]) * 2,
        (int)p2.b - std_o[2],
    };

    double dot_product = vector_1[0] * vector_2[0] + vector_1[1] * vector_2[1] +
                         vector_1[2] * vector_2[2];
    double mod_product =
        sqrt((vector_1[0] * vector_1[0] + vector_1[1] * vector_1[1] +
              vector_1[2] * vector_1[2]) *
             (vector_2[0] * vector_2[0] + vector_2[1] * vector_2[1] +
              vector_2[2] * vector_2[2]));

    return dot_product / mod_product;
}

int get_score_by_cos_sim(double cos_a) {
    if (cos_a > 1.0 || cos_a < -1.0) {
        LOG("ERROR wrong cos value input %f, please check it ", cos_a);
        return 0;
    }
    if (cos_a < 0.5) {
        return 0;
    } // 超过45度不计入
    return 200 * (cos_a - 0.5);
}

float saturation(int* nums) {
    int max = nums[0];
    int min = nums[0];
    for (int i = 1; i < 3; i++) {
        if (nums[i] > max) {
            max = nums[i];
        } else if (nums[i] < min) {
            min = nums[i];
        }
    }
    return (1.0 * max - min) / max;
}

enum skill_t stat_item_pix(uint8_t *input, int buffer_stride,
                           uint8_t *stat_buffer) {
    // char dump_input_fn[50];
    int pix_num = 0;

    // 用于判断该区域可能是不同图标的得分
    int score_r = 0;
    int score_b = 0;
    int score_y = 0;

    // 记录该区域内白色像素点的数量，用于判断它是一个图标的可能
    int num_w = 0;

    int num_r = 0;
    int num_b = 0;
    int num_y = 0;

    int edge_avg_color[3] = {0, 0, 0}; 

    struct pix_color_t *p_pix = NULL;
    struct pix_color_t *p_stat_pix = NULL;
    // int sc_item_stride = sample_sc_item_size * PIX_FMT_SIZ;
    int edge_pix_cnt = 0;

    // 忽略每个区域边缘像素， 防止数组越界
    for (int i = STAT_BORDER; i < sample_sc_item_size - STAT_BORDER; i++) {
        for (int j = STAT_BORDER; j < sample_sc_item_size - STAT_BORDER; j++) {
            pix_num++;
            int offset = i * buffer_stride + j * PIX_FMT_SIZ;
            p_pix = (struct pix_color_t *)(input + offset);
            if (stat_buffer != NULL) {
                p_stat_pix = (struct pix_color_t *)(stat_buffer + offset);
            }

            int dist_w = squa_euc_dist(*p_pix, STD_W);
            if (dist_w < JUDG_VAL_W) {
                num_w++;
                set_pix_color(p_stat_pix, 0, 0, 0);
            } else {
                /*
                统计周围 BORDER 格内是否有白色判断是否为图标边缘
                如果是图标边缘， 判别它的颜色
                */
                int is_item_edge = 0;
                for (int scan_i = -STAT_BORDER;
                     scan_i <= STAT_BORDER && !is_item_edge; scan_i++) {
                    for (int scan_j = -STAT_BORDER;
                         scan_j <= STAT_BORDER && !is_item_edge; scan_j++) {
                        struct pix_color_t *p_scan_pix =
                            (struct pix_color_t *)(((uint8_t *)p_pix +
                                                    (scan_i * buffer_stride) +
                                                    scan_j * PIX_FMT_SIZ));
                        if (squa_euc_dist(*p_scan_pix, STD_W) < JUDG_VAL_W) {
                            is_item_edge = 1;
                        }
                    }
                }

                if (is_item_edge) {
                    edge_pix_cnt++;
                    edge_avg_color[0] += p_pix->r;
                    edge_avg_color[1] += p_pix->g;
                    edge_avg_color[2] += p_pix->b;

                    double cos_r = cos_sim(*p_pix, STD_R);
                    double cos_b = cos_sim(*p_pix, STD_B);
                    double cos_y = cos_sim(*p_pix, STD_Y);

                    score_r += get_score_by_cos_sim(cos_r);
                    score_b += get_score_by_cos_sim(cos_b);
                    score_y += get_score_by_cos_sim(cos_y);

                    if (cos_r > cos_y && cos_r > cos_b) {
                        num_r++;
                        set_pix_color(p_stat_pix, 0, 255, 255);
                    } else if (cos_b > cos_r && cos_b > cos_y) {
                        num_b++;
                        set_pix_color(p_stat_pix, 255, 255, 0);
                    } else if (cos_y > cos_r && cos_y > cos_b) {
                        num_y++;
                        set_pix_color(p_stat_pix, 0, 0, 255);
                    }
                } else {
                    // 不属于图标范围， 降低亮度
                    if (p_stat_pix != NULL) {
                        p_stat_pix->r = p_stat_pix->r / 2;
                        p_stat_pix->g = p_stat_pix->g / 2;
                        p_stat_pix->b = p_stat_pix->b / 2;
                    }
                }
            }
        }
    }

    if (edge_pix_cnt > 0) {
        edge_avg_color[0] = edge_avg_color[0] / edge_pix_cnt;
        edge_avg_color[1] = edge_avg_color[1] / edge_pix_cnt;
        edge_avg_color[2] = edge_avg_color[2] / edge_pix_cnt;

        LOG("avg_color [%d, %d, %d]", edge_avg_color[0], edge_avg_color[1],
            edge_avg_color[2]);
    } else {
        return SKILL_T_INVALID;
    }

    LOG("pix num: %d edge cnt : %d pixs score (r, b, y, w) %d:%d \t%d:%d "
        "\t%d:%d \t%d",
        pix_num, edge_pix_cnt, num_r, score_r, num_b, score_b, num_y, score_y,
        num_w);

    // 具有一定数量的纯白色像素 判定成技能图标
    if (1.0 * num_w / pix_num > 0.02) {
        enum skill_t res = SKILL_T_INVALID;

        float satura = saturation(edge_avg_color);
        int score_chro = score_r + score_b + score_y;
        float conf_r = 1.0 * score_r / score_chro;
        float conf_b = 1.0 * score_b / score_chro;
        float conf_y = 1.0 * score_y / score_chro;

        if (saturation(edge_avg_color) <= 0.05) {
            res = SKILL_T_W;
        } else {
            if (conf_r > 0.5 && score_r / num_r > MIN_COLOR_AVG_SCORE) {
                res = SKILL_T_R;
            } else if (conf_b > 0.5 && score_b / num_b > MIN_COLOR_AVG_SCORE) {
                res = SKILL_T_B;
            } else if (conf_y > 0.5 && score_y / num_y > MIN_COLOR_AVG_SCORE) {
                res = SKILL_T_Y;
            } else {
                LOG("======  Incorrectly recognizable !!!!!!!");
                res = SKILL_T_W;
            }
        }
        LOG("res : %d, satura:%f conf: r: %f \tb: %f\t y:%f", res, satura,conf_r, conf_b,
            conf_y);

        return res;
    }
    return SKILL_T_INVALID;
}

int fill_input_buffer(void *input_buffer, int input_buffer_w,
                      int input_buffer_h, int f_offset) {
    LOG("fill_input_buffer %d, %d ", input_buffer_w, input_buffer_h);

    if (sc_buffer == NULL) {
        LOG("sc_buffer is NULL");
        return 1;
    }

    uint32_t f_stride = PIX_FMT_SIZ * sample_sc_w;
    uint8_t *dest = (uint8_t*)input_buffer;
    for (int i = 0; i < sample_sc_item_size; i++) {
        memcpy(dest, (uint8_t*)sc_buffer + f_offset, input_buffer_w * PIX_FMT_SIZ);
        dest += PIX_FMT_SIZ * input_buffer_w;
        f_offset += f_stride;
    }

    return 0;
}

void dump_buffer(char *filename, void *buffer, int buffer_size) {
    FILE *pf_dump_input = fopen(filename, "ab+");
    fseek(pf_dump_input, 0, SEEK_END);
    fwrite(buffer, buffer_size, 1, pf_dump_input);
    fclose(pf_dump_input);
}

static void *input_buffer;
static void *stat_buffer; // check stat result
static enum skill_t skill_slot[8] = {SKILL_T_INVALID};

enum skill_t *get_skill_slot() { return skill_slot; }

void anal_scr_get_skill_slot(int frame_buffer_fd) {
    LOG("anal_scr_get_skill_slot");
    uint32_t input_buffer_w =
        sample_sc_skill_slot_r - sample_sc_skill_slot_l + sample_sc_item_size;
    uint32_t input_buffer_h = sample_sc_item_size;
    uint32_t input_buffer_size = input_buffer_w * input_buffer_h * PIX_FMT_SIZ;
    uint32_t input_buffer_stride = PIX_FMT_SIZ * input_buffer_w;

    uint32_t f_stride = PIX_FMT_SIZ * sample_sc_w;
    uint64_t f_offset =
        (sample_sc_skill_slot_h - sample_sc_item_size / 2) * f_stride +
        (sample_sc_skill_slot_l - sample_sc_item_size / 2) * PIX_FMT_SIZ;

    if (input_buffer == NULL) {
        input_buffer = malloc(input_buffer_size);
    }

    int ret = fill_input_buffer(input_buffer, input_buffer_w, input_buffer_h,
                                f_offset);
    if (ret) {
        LOG("fill_input_buffer failed");
        return;
    }

    if (dump_frame_count < DUMP_FRAME_TOTAL) {
        if (stat_buffer == NULL) {
            stat_buffer = malloc(input_buffer_size);
        }
        memcpy(stat_buffer, input_buffer, input_buffer_size);
    } else {
        if (stat_buffer != NULL) {
            stat_buffer = NULL;
            free(stat_buffer);
        }
    }

    long ts = util_get_timestamp();
    for (int i = 0; i < 8; i++) {
        int offset = sample_sc_item_spac * PIX_FMT_SIZ * i;
        uint8_t *p_item_input = (uint8_t*)input_buffer + offset;
        uint8_t *p_item_stat = NULL;
        if (stat_buffer != NULL) {
            p_item_stat = (uint8_t*)stat_buffer + offset;
        } else {
            p_item_stat = NULL;
        }

        skill_slot[i] =
            stat_item_pix(p_item_input, input_buffer_stride, p_item_stat);
    }

    LOG(" get result timecost %llu", util_get_timestamp() - ts);

    if (stat_buffer != NULL) {
        char dump_fn[50];
        sprintf(dump_fn, PATH_ROOT "dump_stat_%dx%d.rgb24", input_buffer_w,
                input_buffer_h);
        if (dump_frame_count == 0) {
            remove(dump_fn);
        }
        dump_buffer(dump_fn, stat_buffer, input_buffer_size);
#if 1
        // dump input buffer
        sprintf(dump_fn, PATH_ROOT "dump_input_%dx%d.rgb24", sample_sc_w,
                sample_sc_h);
        if (dump_frame_count == 0) {
            remove(dump_fn);
        }
        dump_buffer(dump_fn, sc_buffer, sample_buffer_size);
#endif
    }

    dump_frame_count++;

    char skill_slot_res_msg[100] = "skill_slot [";
    for (int i = 0; i < 8; i++) {
        char skill_item[5];
        sprintf(skill_item, "%d,", skill_slot[i]);
        strcat(skill_slot_res_msg, skill_item);
    }
    strcat(skill_slot_res_msg, "]");

    LOG("%s", skill_slot_res_msg);
}

void *screencapfunction(void *args) {
    LOG("start screencapfunction");
    while (!stop_anal_flag) {
        static uint32_t sc_frame_count = 0;
        long ts_last = 0;
        long ts_new = 0;
        float fps = 0;
        int reded_size = 0;
        int tmp_sz = 0;
        while (reded_size < sample_buffer_size) {
            tmp_sz = read(fd_pipe_fifo, (uint8_t*)sc_buffer + reded_size,
                          sample_buffer_size - reded_size);
            if (tmp_sz > 0) {
                reded_size += tmp_sz;
            }
        }
        sc_frame_count++;
        if (sc_frame_count % 60 == 0) {
            ts_new = util_get_timestamp();
            fps = 1.0 * 60 / (ts_new - ts_last);
            LOG("frame cnt: %d, fps: %f", sc_frame_count, fps);
            ts_last = ts_new;
        }
    }

    //flush pipe
    while(read(fd_pipe_fifo, sc_buffer, sample_buffer_size));
    LOG("pipe flushed");
    close(fd_pipe_fifo);
    remove(FIFO_FN);

    return NULL;
}

void *scranalfunction(void *args) {
    LOG("start scranalfunction");
    while (!stop_anal_flag) {
        anal_scr_get_skill_slot(fd_pipe_fifo);
        usleep(100 * 1000);
    }

    LOG("stop scranal_thrd");

    return NULL;
}

void init_sc_anal_params() {
    sample_sc_w = origin_sc_w / DOWN_SCALE_RATE;
    sample_sc_h = origin_sc_h / DOWN_SCALE_RATE;

    sample_sc_skill_slot_l = origin_sc_skill_slot_l / DOWN_SCALE_RATE;
    sample_sc_skill_slot_r = origin_sc_skill_slot_r / DOWN_SCALE_RATE;
    sample_sc_skill_slot_h = origin_sc_skill_slot_h / DOWN_SCALE_RATE;

    sample_sc_item_spac = origin_sc_item_spac / DOWN_SCALE_RATE;

    sample_sc_item_size = sample_sc_item_spac / 2;

    sample_buffer_size = sample_sc_w * sample_sc_h * PIX_FMT_SIZ;

    std_o[0] = ((int)STD_R.r + STD_B.r + STD_Y.r) / 3;
    std_o[1] = ((int)STD_R.g + STD_B.g + STD_Y.g) / 3;
    std_o[2] = ((int)STD_R.b + STD_B.b + STD_Y.b) / 3;

    LOG(
        "init_sc_anal_params loaded"
        "sc_w x sc_h: %d x %d\n"
        "skillslot l, h, r (%d, %d, %d)"
        "sample info:"
        "%d x %d"
        "l, h, r (%d, %d, %d)",
        origin_sc_w, origin_sc_h,
        origin_sc_skill_slot_l, origin_sc_skill_slot_h, origin_sc_skill_slot_r,
        sample_sc_w, sample_sc_h,
        sample_sc_skill_slot_l, sample_sc_skill_slot_h, sample_sc_skill_slot_r
    );
}

int start_scr_anal() {
    LOG("start_scr_anal");
    stop_anal_flag = 0;
    init_sc_anal_params();

    unlink(FIFO_FN);
    mkfifo(FIFO_FN, 0777);
    fd_pipe_fifo = open(FIFO_FN, O_RDONLY | O_NONBLOCK);
    if (fd_pipe_fifo < 0) {
        LOG("pipe create failed");
        return 1;
    }

    sc_buffer = malloc(sample_buffer_size);

    pthread_create(&scrcap_thrd, NULL, screencapfunction, NULL);
    pthread_create(&scranal_thrd, NULL, scranalfunction, NULL);

    char resolution_arg[20];
    sprintf(resolution_arg, "%dx%d", sample_sc_w, sample_sc_h);

    sc_cap_pid = fork();
    if (sc_cap_pid == 0) {
        execl("/system/bin/screenrecord", "screenrecord", "--size", resolution_arg, "--output-format", "raw-frames", FIFO_FN, NULL);
    }

    return 0;
}

void stop_scr_anal() {
    if (sc_cap_pid > 0) {
        LOG("KILL PID %d", sc_cap_pid);
        kill(sc_cap_pid, SIGKILL);
    }
    stop_anal_flag = 1;
}