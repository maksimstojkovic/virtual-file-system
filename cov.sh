#!/usr/bin/env bash

# Script for generating coverage data on filesystem

cp *.h *.c cov

cd cov

gcc -O0 -std=gnu11 -fsanitize=address -Wall -Werror -g --coverage -o runtest runtest.c myfilesystem.c helper.c arr.c -lfuse -lm -lpthread
./runtest

lcov -c -d . -o lcov.info
genhtml lcov.info -o cov-html