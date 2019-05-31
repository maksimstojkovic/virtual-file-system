#!/usr/bin/env bash

# Script for generating HTML coverage reports on filesystem tests
# A separate directory is used to prevent conflicts when using lcov

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

# Generate coverage report
lcov -c -d . -o lcov.info
genhtml lcov.info -o cov-html