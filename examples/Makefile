CXX=g++
CXXFLAGS=-Wall -Wextra -g -O1 -std=c++11 -pthread

TARGETS=thread_example regex_example producer-consumer
PC_SRC = producer-consumer.cpp BoundedBuffer.cpp

all: $(TARGETS)

thread_example: thread_example.cpp
	$(CXX) $^ -o $@ $(CXXFLAGS)

regex_example: regex_example.cpp
	$(CXX) $^ -o $@ $(CXXFLAGS)

producer-consumer: $(PC_SRC) BoundedBuffer.hpp
	$(CXX) $^ -o $@ $(CXXFLAGS)

clean:
	rm -f $(TARGETS)
