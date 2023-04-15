
void input_tap_event(int x, int y);

int start_key_monitor();

void stop_key_monitor();

void set_key_listener(void (*listener)(struct input_event event));