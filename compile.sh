#!/bin/bash
# Compilation script for Assignment 4 - Bayesian Network Parameter Learning

echo "Compiling Bayesian Network parameter learner..."

# Compile with C++11 standard and optimizations
# g++ -std=c++11 -O2 startup_code.cpp -o em_solver'

g++ -std=c++11 -O2 learn_bn.cpp -o em_solver

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "Executable: em_solver"
else
    echo "Compilation failed!"
    exit 1
fi