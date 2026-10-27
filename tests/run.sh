#!/bin/sh
# Build and run the native core tests (needs only a C++17 compiler).
set -e
cd "$(dirname "$0")"
${CXX:-g++} -std=c++17 -Wall -Wextra -O1 -fsanitize=address,undefined core_test.cpp -o /tmp/silicate_core_test
/tmp/silicate_core_test
