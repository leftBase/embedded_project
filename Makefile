CC ?= gcc
TARGET = racing_game

CFLAGS ?= -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -Iinclude
LDFLAGS ?= -pthread

SRCS = src/main.c src/game.c src/event.c src/render.c src/serial.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
