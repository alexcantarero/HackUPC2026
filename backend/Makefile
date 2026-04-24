CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET   = bin/solver
SRCDIR   = src

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/core/collision.cpp \
       $(SRCDIR)/io/parser.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET) --case ../data/input/Case0

TEST_SRCS = tests/test_collision.cpp \
            $(SRCDIR)/core/collision.cpp \
            $(SRCDIR)/io/parser.cpp

test: $(TEST_SRCS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_collision $(TEST_SRCS)
	./bin/test_collision

clean:
	rm -f $(OBJS) bin/solver bin/test_collision

.PHONY: all run test clean
