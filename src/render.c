#include "render.h"

#include <stdio.h>
#include <string.h>

#define PANEL_INNER_WIDTH 34
#define PANEL_TOTAL_WIDTH 36
#define PANEL_LINES (ROAD_HEIGHT + 7)

static const int lane_center[LANE_COUNT] = {6, 17, 28};

static const char *item_to_string(ItemType item) {
    switch (item) {
        case ITEM_RED:
            return "RED";
        case ITEM_GREEN:
            return "GREEN";
        case ITEM_BLUE:
            return "BLUE";
        case ITEM_NONE:
        default:
            return "NONE";
    }
}

static void make_life_string(int life, char *out, int out_size) {
    int i;
    int pos = 0;

    if (life <= 0) {
        snprintf(out, out_size, "[ ]");
        return;
    }

    for (i = 0; i < life && pos < out_size - 4; i++) {
        pos += snprintf(out + pos, out_size - pos, "[*]");
    }

    out[pos] = '\0';
}

static void center_text(char *out, const char *text, int width) {
    int len = (int)strlen(text);
    int left;
    int right;
    int pos = 0;
    int i;

    if (len > width) {
        len = width;
    }

    left = (width - len) / 2;
    right = width - len - left;

    for (i = 0; i < left; i++) {
        out[pos++] = ' ';
    }

    for (i = 0; i < len; i++) {
        out[pos++] = text[i];
    }

    for (i = 0; i < right; i++) {
        out[pos++] = ' ';
    }

    out[pos] = '\0';
}

static void make_border(char *out, const char *title, char fill) {
    char inner[PANEL_INNER_WIDTH + 1];
    int i;

    for (i = 0; i < PANEL_INNER_WIDTH; i++) {
        inner[i] = fill;
    }
    inner[PANEL_INNER_WIDTH] = '\0';

    if (title != NULL && title[0] != '\0') {
        char title_text[64];
        int len;
        int start;
        int j;

        snprintf(title_text, sizeof(title_text), " %s ", title);
        len = (int)strlen(title_text);
        start = (PANEL_INNER_WIDTH - len) / 2;

        if (start < 0) {
            start = 0;
        }

        for (j = 0; j < len && start + j < PANEL_INNER_WIDTH; j++) {
            inner[start + j] = title_text[j];
        }
    }

    snprintf(out, PANEL_TOTAL_WIDTH + 1, "+%s+", inner);
}

static void make_content_line(char *out, const char *content) {
    char inner[PANEL_INNER_WIDTH + 1];

    center_text(inner, content, PANEL_INNER_WIDTH);
    snprintf(out, PANEL_TOTAL_WIDTH + 1, "|%s|", inner);
}

static void put_text(char *line, int x, const char *text) {
    int i;
    int len = (int)strlen(text);

    if (x < 0) {
        return;
    }

    for (i = 0; i < len; i++) {
        if (x + i >= 0 && x + i < PANEL_INNER_WIDTH) {
            line[x + i] = text[i];
        }
    }
}

static void put_centered(char *line, int center, const char *text) {
    int len = (int)strlen(text);
    int x = center - len / 2;

    put_text(line, x, text);
}

static int rock_art_for_row(int rock_y, int row, char *out, int out_size) {
    int rel = row - rock_y;

    if (rel == -1) {
        snprintf(out, out_size, "[X]");
        return 1;
    }

    if (rel == 0) {
        snprintf(out, out_size, "[X]");
        return 1;
    }

    if (rel == 1) {
        snprintf(out, out_size, "[X]");
        return 1;
    }

    return 0;
}

static void make_road_line(const GameState *game, int player_index, int row, char *out) {
    char inner[PANEL_INNER_WIDTH + 1];
    char art[16];
    int i;
    const Player *player = &game->players[player_index];

    memset(inner, ' ', PANEL_INNER_WIDTH);
    inner[PANEL_INNER_WIDTH] = '\0';

    inner[11] = '|';
    inner[22] = '|';

    for (i = 0; i < MAX_ROCKS; i++) {
        const Rock *rock = &game->rocks[player_index][i];

        if (!rock->active) {
            continue;
        }

        if (rock_art_for_row(rock->y, row, art, sizeof(art))) {
            if (rock->lane >= 0 && rock->lane < LANE_COUNT) {
                put_centered(inner, lane_center[rock->lane], art);
            }
        }
    }

    if (row == ROAD_HEIGHT - 2) {
        char car_top[8];

        snprintf(car_top, sizeof(car_top), " %c ", player_index == PLAYER_1 ? 'A' : 'B');
        put_centered(inner, lane_center[player->lane], car_top);
    }

    if (row == ROAD_HEIGHT - 1) {
        put_centered(inner, lane_center[player->lane], "[_]");
    }

    snprintf(out, PANEL_TOTAL_WIDTH + 1, "|%s|", inner);
}

static void build_player_panel(const GameState *game, int player_index, char lines[PANEL_LINES][PANEL_TOTAL_WIDTH + 1]) {
    char title[32];
    char status[128];
    char life[32];
    char item[96];
    int idx = 0;
    int r;
    const Player *player = &game->players[player_index];

    snprintf(title, sizeof(title), "PLAYER %d", player_index + 1);
    make_border(lines[idx++], title, '=');

    make_life_string(player->life, life, sizeof(life));
    snprintf(status, sizeof(status), "LIFE:%s   SCORE:%05d", life, player->score);
    make_content_line(lines[idx++], status);

    if (player->item == ITEM_GREEN) {
        snprintf(item, sizeof(item), "ITEM:%s  STACK:%d  TIME:%d",
                 item_to_string(player->item), player->green_stack, player->item_timer);
    } else {
        snprintf(item, sizeof(item), "ITEM:%s  TIME:%d",
                 item_to_string(player->item), player->item_timer);
    }
    make_content_line(lines[idx++], item);

    make_border(lines[idx++], NULL, '-');
    make_content_line(lines[idx++], "LANE 1    LANE 2    LANE 3");
    make_border(lines[idx++], NULL, '-');

    for (r = 0; r < ROAD_HEIGHT; r++) {
        make_road_line(game, player_index, r, lines[idx++]);
    }

    make_border(lines[idx++], NULL, '=');
}

static void build_message_panel(const char *title, const char *message, char lines[PANEL_LINES][PANEL_TOTAL_WIDTH + 1]) {
    int i;

    make_border(lines[0], title, '=');

    for (i = 1; i < PANEL_LINES - 1; i++) {
        if (i == PANEL_LINES / 2) {
            make_content_line(lines[i], message);
        } else {
            make_content_line(lines[i], "");
        }
    }

    make_border(lines[PANEL_LINES - 1], NULL, '=');
}

static void build_game_over_panels(const GameState *game, char left[PANEL_LINES][PANEL_TOTAL_WIDTH + 1], char right[PANEL_LINES][PANEL_TOTAL_WIDTH + 1]) {
    char message[64];

    if (game->winner == PLAYER_1) {
        snprintf(message, sizeof(message), "PLAYER 1 WIN!");
    } else if (game->winner == PLAYER_2) {
        snprintf(message, sizeof(message), "PLAYER 2 WIN!");
    } else {
        snprintf(message, sizeof(message), "DRAW!");
    }

    build_message_panel("GAME OVER", message, left);
    build_message_panel("GAME OVER", "PRESS START TO RESTART", right);
}

int render_init(void) {
    printf("\033[2J\033[?25l");
    fflush(stdout);
    return 0;
}

void render_game(const GameState *game) {
    char left[PANEL_LINES][PANEL_TOTAL_WIDTH + 1];
    char right[PANEL_LINES][PANEL_TOTAL_WIDTH + 1];
    int i;

    printf("\033[H");

    if (game->state == GAME_READY) {
        build_message_panel("READY", "PRESS START", left);
        build_message_panel("CONTROLS", "Q6+M4 BUTTONS READY", right);
    } else if (game->state == GAME_PAUSED) {
        build_message_panel("PLAYER 1", "PAUSED", left);
        build_message_panel("PLAYER 2", "PAUSED", right);
    } else if (game->state == GAME_OVER) {
        build_game_over_panels(game, left, right);
    } else {
        build_player_panel(game, PLAYER_1, left);
        build_player_panel(game, PLAYER_2, right);
    }

    for (i = 0; i < PANEL_LINES; i++) {
        printf("%s  %s\n", left[i], right[i]);
    }

    printf("\n");
    printf("tick:%d lcd:%d spawn:%d%% rock_move:%d\n",
           game->tick_count, game->lcd, game->spawn_chance, game->rock_move_ticks);
    printf("keys: a/d/q=P1 s=start p=pause j/l/o=P2 x=quit\n");
    fflush(stdout);
}

void render_shutdown(void) {
    printf("\033[?25h\033[0m\n");
    fflush(stdout);
}
