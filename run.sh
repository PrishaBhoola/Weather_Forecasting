#!/bin/bash
# Usage: ./run.sh hailfinder.bif records.dat

# Check if correct number of arguments provided
if [ $# -ne 2 ]; then
    echo "Usage: ./run.sh <network_file.bif> <data_file.dat>"
    echo "Example: ./run.sh hailfinder.bif records.dat"
    exit 1
fi

# Get input files from command line arguments
NETWORK_FILE=$1
DATA_FILE=$2

# Check if input files exist
if [ ! -f "$NETWORK_FILE" ]; then
    echo "Error: Network file '$NETWORK_FILE' not found!"
    exit 1
fi

if [ ! -f "$DATA_FILE" ]; then
    echo "Error: Data file '$DATA_FILE' not found!"
    exit 1
fi

# Check if executable exists
if [ ! -f "./em_solver" ]; then
    echo "Error: Executable 'em_solver' not found!"
    echo "Please run compile.sh first"
    exit 1
fi

echo "Starting Bayesian Network parameter learning..."
echo "Network file: $NETWORK_FILE"
echo "Data file: $DATA_FILE"
echo "Output file: solved_hailfinder.bif"
echo ""

# Run the parameter learner with command-line arguments
./em_solver "$NETWORK_FILE" "$DATA_FILE" solved_hailfinder.bif

# Check if output was created
if [ -f "solved_hailfinder.bif" ]; then
    echo ""
    echo "Learning complete! Output written to: solved_hailfinder.bif"
    exit 0
else
    echo ""
    echo "Error: Output file not created!"
    exit 1
fi