CC       = gcc
CFLAGS   = -Wall -Wextra -g

SRCS     = main.c command.c help.c tree.c print.c
OBJS     = $(SRCS:.c=.o)

TARGET   = ssu_ext2

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c header.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f $(TARGET) $(OBJS)

