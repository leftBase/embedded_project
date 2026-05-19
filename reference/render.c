#include "render.h"

#include <stdio.h>
#include <string.h>

#define PANEL_INNER_WIDTH 56
#define PANEL_TOTAL_WIDTH 58
#define PANEL_LINES (ROAD_HEIGHT + 7)

static const int lane_center[LANE_COUNT] = {9, 28, 47};

static void clear_screen(void) {
    printf("\033[H");
}

static void hide_cursor(void) {
    printf("\033[?25l");
}

static void show_cursor(void) {
    printf("\033[?25h");
}

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

    for (i = 0; i < life && pos < out_size - 4; i++) {
        pos += snprintf(out + pos, out_size - pos, "[*]");
    }

    if (life <= 0) {
        snprintf(out, out_size, "[ ]");
        return;
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

    if (title != NULL && strlen(title) > 0) {
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

static void make_left_content_line(char *out, const char *content) {
    char inner[PANEL_INNER_WIDTH + 1];
    int len = (int)strlen(content);
    int i;

    memset(inner, ' ', PANEL_INNER_WIDTH);
    inner[PANEL_INNER_WIDTH] = '\0';

    if (len > PANEL_INNER_WIDTH) {
        len = PANEL_INNER_WIDTH;
    }

    for (i = 0; i < len; i++) {
        inner[i] = content[i];
    }

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

static int obstacle_art_for_row(int obs_y, int row, char *out, int out_size) {
    int rel = row - obs_y;

    if (rel == -1) {
        snprintf(out, out_size, "+---+");
        return 1;
    }

    if (rel == 0) {
        snprintf(out, out_size, "|###|");
        return 1;
    }

    if (rel == 1) {
        snprintf(out, out_size, "+---+");
        return 1;
    }

    return 0;
}

static void make_road_line(const Game *game, int player_index, int row, char *out) {
    char inner[PANEL_INNER_WIDTH + 1];
    int i;
    int lane;
    char art[16];
    const Player *player = &game->players[player_index];

    memset(inner, ' ', PANEL_INNER_WIDTH);
    inner[PANEL_INNER_WIDTH] = '\0';

    inner[18] = '|';
    inner[37] = '|';

    for (i = 0; i < MAX_OBSTACLES; i++) {
        const Obstacle *obs = &game->obstacles[player_index][i];

        if (!obs->active) {
            continue;
        }

        if (obstacle_art_for_row(obs->y, row, art, sizeof(art))) {
            lane = obs->lane;

            if (lane >= 0 && lane < LANE_COUNT) {
                put_centered(inner, lane_center[lane], art);
            }
        }
    }

    if (row == ROAD_HEIGHT - 2) {
        char car_top[8];

        snprintf(car_top, sizeof(car_top), "/%c\\", player_index == PLAYER_1 ? 'A' : 'B');
        put_centered(inner, lane_center[player->lane], car_top);
    }

    if (row == ROAD_HEIGHT - 1) {
        put_centered(inner, lane_center[player->lane], "|___|");
    }

    snprintf(out, PANEL_TOTAL_WIDTH + 1, "|%s|", inner);
}

static void build_player_panel(const Game *game, int player_index, char lines[PANEL_LINES][PANEL_TOTAL_WIDTH + 1]) {
    char title[32];
    char status[128];
    char life[32];
    char item[64];
    int idx = 0;
    int r;
    const Player *player = &game->players[player_index];

    snprintf(title, sizeof(title), "PLAYER %d", player_index + 1);
    make_border(lines[idx++], title, '=');

    make_life_string(player->life, life, sizeof(life));
    snprintf(status, sizeof(status), "LIFE:%s   SCORE:%05d", life, player->score);
    make_content_line(lines[idx++], status);

    if (player->mash_active) {
        snprintf(item, sizeof(item), "ITEM:%s  MASH:%d/%d", item_to_string(player->mash_item), player->mash_count, ITEM_MASH_LIMIT);
    } else {
        snprintf(item, sizeof(item), "ITEM:%s", item_to_string(player->item));
    }
    make_content_line(lines[idx++], item);

    make_border(lines[idx++], NULL, '-');
    make_content_line(lines[idx++], "LANE 1          LANE 2          LANE 3");
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

static void build_game_over_panels(const Game *game, char left[PANEL_LINES][PANEL_TOTAL_WIDTH + 1], char right[PANEL_LINES][PANEL_TOTAL_WIDTH + 1]) {
    char msg[64];

    if (game->winner == PLAYER_1) {
        snprintf(msg, sizeof(msg), "PLAYER 1 WIN!");
    } else if (game->winner == PLAYER_2) {
        snprintf(msg, sizeof(msg), "PLAYER 2 WIN!");
    } else {
        snprintf(msg, sizeof(msg), "DRAW!");
    }

    build_message_panel("GAME OVER", msg, left);
    build_message_panel("GAME OVER", "PRESS START TO RESTART", right);
}

void render_init(void) {
    printf("\033[2J");
    hide_cursor();
    fflush(stdout);
}

void render_draw(const Game *game) {
    char left[PANEL_LINES][PANEL_TOTAL_WIDTH + 1];
    char right[PANEL_LINES][PANEL_TOTAL_WIDTH + 1];
    int i;

    clear_screen();

    if (game->state == STATE_READY) {
        build_message_panel("READY", "PRESS START", left);
        build_message_panel("CONTROLS", "A/D Q | S | J/L O", right);
    } else if (game->state == STATE_PAUSED) {
        build_message_panel("PLAYER 1", "PAUSED", left);
        build_message_panel("PLAYER 2", "PAUSED", right);
    } else if (game->state == STATE_GAME_OVER) {
        build_game_over_panels(game, left, right);
    } else {
        build_player_panel(game, PLAYER_1, left);
        build_player_panel(game, PLAYER_2, right);
    }

    for (i = 0; i < PANEL_LINES; i++) {
        printf("%s  %s\n", left[i], right[i]);
    }

    printf("\n");
    printf("Controls: a/d=1P move, q=1P func, s=start, p=pause, j/l=2P move, o=2P func, x=quit\n");
    fflush(stdout);
}

void render_close(void) {
    show_cursor();
    printf("\033[0m\n");
    fflush(stdout);
}