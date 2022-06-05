export CXX=clang++

all: build-tests

spell.hh.gch: spell.hh
	$(CXX) -std=c++20 spell.hh -g

build-tests: spell.hh.gch
	$(MAKE) -C tests/programs
	$(MAKE) -C tests

clean:
	rm -f spell.hh.gch
	$(MAKE) -C tests/programs clean
	$(MAKE) -C tests clean

.PHONY: build-tests clean
