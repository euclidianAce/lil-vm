#!/usr/bin/env sh

set -xe

cc -Wall -Wextra -Werror -pedantic -std=c99 vm.c common_ports.c sv.c $1.c -o $1
