CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?= 

SRCS = $(wildcard src/shell/*.cpp) \
       $(wildcard src/ui/*.cpp) \
       $(wildcard src/editor/*.cpp) \
       $(wildcard src/plugin/*.cpp) \
       $(wildcard src/config/*.cpp) \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = bin/aswell

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

bin:
	mkdir -p bin

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/aswell

test:
	./tests/run_all_tests.sh

.PHONY: all clean install test
