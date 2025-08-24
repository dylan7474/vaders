CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer)
LDFLAGS=$(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm

all: vaders

vaders: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f vaders
