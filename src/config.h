#define PATH_ROOT "/data/local/temp/"
#define INPUT_SOURCE "/dev/input/event4"

#define PARAMS_FILE PATH_ROOT "init_params.bin"

#define DEFAULT_ORIGIN_SC_W 2560
#define DEFAULT_ORIGIN_SC_H 1440
#define DEFAULT_ORIGIN_SC_SKILL_SLOT_L 1431
#define DEFAULT_ORIGIN_SC_SKILL_SLOT_R 2484
#define DEFAULT_ORIGIN_SC_SKILL_SLOT_H 1060
#define DEFAULT_ORIGIN_SC_SKILL_ITEM_SPAC                                      \
    (DEFAULT_ORIGIN_SC_SKILL_SLOT_R - DEFAULT_ORIGIN_SC_SKILL_SLOT_L) / 7

typedef struct {
    int sc_w;
    int sc_h;

    int sc_skill_slot_l;
    int sc_skill_slot_h;
    int sc_skill_slot_r;
} InitParams;

extern int origin_sc_w;
extern int origin_sc_h;

extern int origin_sc_skill_slot_l;
extern int origin_sc_skill_slot_h;
extern int origin_sc_skill_slot_r;

extern int origin_sc_item_spac;