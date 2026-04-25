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
TEST_SRCS = tests/test_collision.cpp \
            $(SRCDIR)/core/collision.cpp \
            $(SRCDIR)/io/parser.cpp

TEST_GA_SRCS = tests/test_ga_ortho.cpp \
               $(SRCDIR)/solvers/genetic_algorithm.cpp \
               $(SRCDIR)/solvers/ga_ortho.cpp \
               $(SRCDIR)/core/collision.cpp

TEST_GA_ANGLE_SRCS = tests/test_ga_angle.cpp \
                     $(SRCDIR)/solvers/genetic_algorithm.cpp \
                     $(SRCDIR)/solvers/ga_angle.cpp \
                     $(SRCDIR)/core/collision.cpp

test: $(TEST_SRCS) $(TEST_GA_SRCS) $(TEST_GA_ANGLE_SRCS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_collision $(TEST_SRCS)
	./bin/test_collision
	$(CXX) $(CXXFLAGS) -o bin/test_ga_ortho $(TEST_GA_SRCS)
	./bin/test_ga_ortho
	$(CXX) $(CXXFLAGS) -o bin/test_ga_angle $(TEST_GA_ANGLE_SRCS)
	./bin/test_ga_angle

clean:
	rm -f $(OBJS) bin/solver bin/test_collision bin/test_ga_ortho bin/test_ga_angle

.PHONY: all run test test_collision test_score test_greedy clean
