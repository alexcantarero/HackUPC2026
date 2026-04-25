CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET   = bin/solver
SRCDIR   = src

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/core/collision.cpp \
       $(SRCDIR)/io/parser.cpp \
       $(SRCDIR)/solvers/ga_ortho.cpp

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

TEST_GA_SRCS = tests/test_ga_ortho.cpp \
               $(SRCDIR)/solvers/ga_ortho.cpp \
               $(SRCDIR)/core/collision.cpp

test: $(TEST_SRCS) $(TEST_GA_SRCS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_collision $(TEST_SRCS)
	./bin/test_collision
	$(CXX) $(CXXFLAGS) -o bin/test_ga_ortho $(TEST_GA_SRCS)
	./bin/test_ga_ortho

clean:
	rm -f $(OBJS) bin/solver bin/test_collision

.PHONY: all run test clean
