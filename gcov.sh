#!/usr/bin/env bash

# Script for outputting gcov coverage data on filesystem tests
# A separate directory is used to prevent conflicts when using gcov

# Create directory "cov" and copy .h and .c files
mkdir -p cov
cp *.h *.c cov

# Move into directory created
cd cov

# Compile program
gcc -O0 -std=gnu11 -fsanitize=address -Wall -Werror -g -fprofile-arcs -ftest-coverage \
-o runtest runtest.c myfilesystem.c helper.c arr.c -lfuse -lm -lpthread

# Run program
./runtest

# Generate coverage data
gcov runtest.c myfilesystem.c helper.c arr.c