CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET   = bin/solver
SRCDIR   = src

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/core/collision.cpp \
       $(SRCDIR)/core/score.cpp \
       $(SRCDIR)/io/parser.cpp \
       $(SRCDIR)/solvers/greedy.cpp \
	$(SRCDIR)/solvers/genetic_algorithm.cpp \
	$(SRCDIR)/solvers/ga_angle.cpp \
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

# ── Tests ──────────────────────────────────────────────────────────────────

test_collision: tests/test_collision.cpp \
                $(SRCDIR)/core/collision.cpp \
                $(SRCDIR)/io/parser.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_collision $^
	./bin/test_collision

test_score: tests/test_score.cpp \
            $(SRCDIR)/core/score.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_score $^
	./bin/test_score

test_greedy: tests/test_greedy.cpp \
             $(SRCDIR)/core/collision.cpp \
             $(SRCDIR)/core/score.cpp \
             $(SRCDIR)/solvers/greedy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_greedy $^
	./bin/test_greedy

test: test_collision test_score test_greedy

clean:
	rm -f $(OBJS) \
	      bin/solver bin/test_collision bin/test_score bin/test_greedy bin/test_ga_ortho bin/test_ga_angle

.PHONY: all run test test_collision test_score test_greedy clean
