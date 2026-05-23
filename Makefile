CC ?= gcc
TARGET = racing_game

CFLAGS ?= -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -DENABLE_DEBUG_LOG -Iinclude
LDFLAGS ?= -pthread

ifeq ($(SIMULATOR),1)
CFLAGS += -DSIMULATOR
SRCS = src/main.c src/game.c src/event.c src/render.c src/debug.c src/simulator.c
else
SRCS = src/main.c src/game.c src/event.c src/render.c src/serial.c src/hardware.c src/debug.c
endif
OBJS = $(SRCS:.c=.o)

.PHONY: sim clean

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

sim:
	$(MAKE) SIMULATOR=1 $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
