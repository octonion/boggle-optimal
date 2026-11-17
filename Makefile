CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS ?=

.PHONY: all test clean benchmark
all: boggle-optimal
boggle-optimal: src/main.cpp src/pattern_bound.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp -o $@
tests/test-workspace: tests/test_workspace.cpp src/main.cpp src/pattern_bound.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_workspace.cpp -o $@
test: boggle-optimal tests/test-workspace
	python3 tests/test_score.py ./boggle-optimal
	python3 tests/test_pattern_bound.py ./boggle-optimal
	./tests/test-workspace tests/words-small.txt
benchmark: boggle-optimal
	./boggle-optimal bench tests/words-small.txt perslatgsineters
clean:
	rm -f boggle-optimal tests/test-workspace
