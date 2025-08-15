CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2)
LDFLAGS=$(shell pkg-config --libs sdl2) -lm

all: main

main: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f main
