CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror

test: test_stats_util
	./test_stats_util

test_stats_util: test_stats_util.cpp ../../src/stats_util.h
	$(CXX) $(CXXFLAGS) -o $@ test_stats_util.cpp

clean:
	rm -f test_stats_util

.PHONY: test clean
