CXX?=clang++
CXXFLAGS?=-std=c++20 -I..

SRC=$(wildcard *.cc)
BIN=$(patsubst %.cc,%.exe,$(SRC))

all: $(BIN)

%.exe: %.cc ../spell.hh
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(BIN)

.PHONY: all clean
