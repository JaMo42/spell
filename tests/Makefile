CXX?=clang++
CXXFLAGS?=-std=c++20 -I.. -gdwarf-4

SRC=$(wildcard *.cc)
BIN=$(patsubst %.cc,%.exe,$(SRC))

all: $(BIN)

%.exe: %.cc ../spell.hh
	$(CXX) $(CXXFLAGS) -o $@ $< -g

clean:
	rm -f $(BIN)

.PHONY: all clean
