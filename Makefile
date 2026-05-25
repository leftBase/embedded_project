CC ?= gcc
TARGET ?= racing_game

SRC_DIR := src
INC_DIR := include

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

MODE := $(if $(filter 1,$(SIMULATOR)),sim,board)
BUILD_DIR := build/$(MODE)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all sim clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

sim:
	$(MAKE) SIMULATOR=1 $(TARGET)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)
