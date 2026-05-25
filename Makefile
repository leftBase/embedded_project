CC ?= gcc
TARGET ?= racing_game

SRC_DIR := src
INC_DIR := include

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)

CPPFLAGS ?= -I$(INC_DIR)
CFLAGS ?= -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -DENABLE_DEBUG_LOG
LDFLAGS ?= -pthread
LDLIBS ?=

ifeq ($(SIMULATOR),1)
CFLAGS += -DSIMULATOR
SRCS = src/main.c src/game.c src/event.c src/render.c src/debug.c src/simulator.c
else
SRCS = src/main.c src/game.c src/event.c src/render.c src/serial.c src/hardware.c src/debug.c
endif
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)


.PHONY: sim clean

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

sim:
	$(MAKE) SIMULATOR=1 $(TARGET)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
