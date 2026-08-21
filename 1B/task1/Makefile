# Task 1: golden measure on the PC.
CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wunused-but-set-variable=0

all: golden_measure

golden_measure: golden_measure.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$@ golden_measure.c -lm

run: bin/golden_measure
	bin/golden_measure

check: run

clean:
	rm -f bin

.PHONY: all run check clean
