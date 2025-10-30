# Makefile for Bayesian Network Parameter Learning

CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

# Targets
all: em_solver format_checker

em_solver: startup_code.cpp
	$(CXX) $(CXXFLAGS) -o em_solver startup_code.cpp

em_solver2: learn_bn.cpp
	$(CXX) $(CXXFLAGS) -o em_solver2 learn_bn.cpp

format_checker: Format_Checker.cpp
	$(CXX) $(CXXFLAGS) -o format_checker Format_Checker.cpp

# Run the EM solver
run: em_solver
	./em_solver

run2 : em_solver2
	./em_solver2

# Check the output format and accuracy
check: format_checker
	./format_checker

# Clean compiled binaries and output
clean:
	rm -f em_solver format_checker solved.bif solver

# Clean only binaries
clean-bin:
	rm -f em_solver format_checker

.PHONY: all run check clean clean-bin
