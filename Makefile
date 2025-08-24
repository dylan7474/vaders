CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2 SDL2_image)
LDFLAGS=$(shell pkg-config --libs sdl2 SDL2_image) -lm

all: main

main: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f main
