#include <stdint.h>

enum skill_t {
    SKILL_T_INVALID = -1,
    SKILL_T_R,
    SKILL_T_B,
    SKILL_T_Y,
    SKILL_T_W
};

struct pix_color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

const static struct pix_color_t STD_R = {228, 174, 186};
const static struct pix_color_t STD_B = {185, 198, 231};
const static struct pix_color_t STD_Y = {219, 186, 165};
const static struct pix_color_t STD_W = {255, 255, 255};

enum skill_t *get_skill_slot();

int start_scr_anal();

void stop_scr_anal();