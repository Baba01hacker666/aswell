CXX ?= g++
HARDENING_FLAGS = -Wall -Wextra -Wpedantic -Werror \
                  -Wconversion -Wsign-conversion \
                  -Wshadow \
                  -Wformat=2 \
                  -Wnull-dereference \
                  -Wold-style-cast \
                  -Wnon-virtual-dtor \
                  -Woverloaded-virtual \
                  -Wimplicit-fallthrough \
                  -fsanitize=address,undefined -g

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
DEMO_TARGET = bin/demo_engine
DEMO_OBJS = $(filter-out src/main.o, $(OBJS)) examples/demo_engine.o

all: $(TARGET) demo

harden:
	$(MAKE) CXXFLAGS="-std=c++20 -O2 $(HARDENING_FLAGS) -Iinclude" LDFLAGS="-fsanitize=address,undefined"

$(TARGET): $(OBJS) | bin
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

demo: $(DEMO_TARGET)

$(DEMO_TARGET): $(DEMO_OBJS) | bin
	$(CXX) $(CXXFLAGS) $(DEMO_OBJS) -o $@ $(LDFLAGS)

bin:
	mkdir -p bin

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) examples/demo_engine.o $(DEMO_TARGET)

install: $(TARGET)
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/aswell

test: $(TARGET)
	CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" ./tests/run_all_tests.sh

test-hardened:
	$(MAKE) test CXXFLAGS="-std=c++20 -O2 $(HARDENING_FLAGS) -Iinclude" LDFLAGS="-fsanitize=address,undefined"

.PHONY: all harden clean install test test-hardened demo
