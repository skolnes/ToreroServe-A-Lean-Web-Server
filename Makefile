CXX=g++
CXXFLAGS=-Wall -Wextra -g -O1 -std=c++11 -pthread -lboost_system -lboost_filesystem

TARGETS=torero-serve
PC_SRC=torero-serve.cpp BoundedBuffer.cpp

all: $(TARGETS)

torero-serve: $(PC_SRC) BoundedBuffer.hpp
	$(CXX) $^ -o $@ $(CXXFLAGS)
clean:
	rm -f $(TARGETS)
