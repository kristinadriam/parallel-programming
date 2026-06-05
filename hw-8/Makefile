CXX      ?= g++
CXXFLAGS := -std=c++17 -pthread -Wall -Wno-pedantic

SET_HDR  := ordered_set.hpp

.PHONY: all run test asan tsan neg clean

all: lin_test lin_asan lin_tsan neg_test

lin_test: linearizability_test.cpp $(SET_HDR)
	$(CXX) $(CXXFLAGS) -O2 linearizability_test.cpp -o $@

lin_asan: linearizability_test.cpp $(SET_HDR)
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address linearizability_test.cpp -o $@

lin_tsan: linearizability_test.cpp $(SET_HDR)
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=thread linearizability_test.cpp -o $@

neg_test: negative_control.cpp
	$(CXX) $(CXXFLAGS) -O2 negative_control.cpp -o $@

test: lin_test
	./lin_test 1000 18

asan: lin_asan
	./lin_asan 400 16

tsan: lin_tsan
	./lin_tsan 220 10

neg: neg_test
	./neg_test 400 25

run: test neg

clean:
	rm -f lin_test lin_asan lin_tsan neg_test
