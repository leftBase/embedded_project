#include "device.h"

void device_init(void) {
}

void device_close(void) {
}

void device_show_item(int player_index, ItemType item) {
    (void)player_index;
    (void)item;
}

void device_show_led(int player_index, LedStatus status) {
    (void)player_index;
    (void)status;
}

void device_play_sound(SoundType sound) {
    (void)sound;
}

void device_show_score(int score) {
    (void)score;
}