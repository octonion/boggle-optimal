CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?=

.PHONY: all test clean benchmark
all: boggle-optimal
boggle-optimal: src/main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
test: boggle-optimal
	python3 tests/test_score.py ./boggle-optimal
benchmark: boggle-optimal
	./boggle-optimal bench tests/words-small.txt perslatgsineters
clean:
	rm -f boggle-optimal
