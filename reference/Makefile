CC = gcc
TARGET = racing_game

SRCS = main.c game.c render.c input_keyboard.c device_stub.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -Wextra -std=c99

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)