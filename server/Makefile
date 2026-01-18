CC = gcc
CFLAGS = -Wall -g -pthread
TARGET = zolik_server
SRCS = main.c server_manager.c client_manager.c protocol.c room_manager.c game_manager.c logger.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)