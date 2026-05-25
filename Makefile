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

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
