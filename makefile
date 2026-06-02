CC=gcc
CFLAGS = -g -Wall -Wextra 
#OBJMODULES = ./deps/miniaudio.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

main: main.c $(OBJMODULES)
	$(CC) $(CFLAGS) $^ -o $@ -lm
