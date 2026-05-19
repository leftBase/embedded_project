#ifndef DEVICE_H
#define DEVICE_H

#include "types.h"

typedef enum {
    SOUND_NONE,
    SOUND_CRASH,
    SOUND_ITEM,
    SOUND_ATTACK,
    SOUND_HEAL,
    SOUND_CLEAR,
    SOUND_GAME_OVER
} SoundType;

typedef enum {
    LED_NONE,
    LED_CRASH,
    LED_HEAL,
    LED_CLEAR,
    LED_WIN
} LedStatus;

void device_init(void);
void device_close(void);

void device_show_item(int player_index, ItemType item);
void device_show_led(int player_index, LedStatus status);
void device_play_sound(SoundType sound);
void device_show_score(int score);

#endif