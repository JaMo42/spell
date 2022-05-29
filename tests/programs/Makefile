CC=clang

SRC=$(wildcard *.c)
BIN=$(patsubst %.c,%.exe,$(SRC))

all: $(BIN)

%.exe: %.c
	$(CC) -o $@ $<

clean:
	rm -f $(BIN)

.PHONY: all clean
