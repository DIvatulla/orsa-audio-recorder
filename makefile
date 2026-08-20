CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude 
MDEF =

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
LIBS := -lasound -lm -lpthread -lmp3lame -lmpg123 -lwebsockets -lssl -lcrypto -lspeexdsp -lfvad

main: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(MDEF) -o $@ $(LIBS)

build/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< $(MDEF) -o $@ $(LIBS)
