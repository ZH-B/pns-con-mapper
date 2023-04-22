struct group_key_stats_t
{
    int LT_hold;
    int LS_hold;
};

void input_tap_event(int x, int y);

int start_key_monitor();

void stop_key_monitor();

void set_key_listener(void (*listener)(struct input_event event));

struct group_key_stats_t *get_group_key_stats();