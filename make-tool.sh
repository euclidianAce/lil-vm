#!/usr/bin/env sh

set -e

run_compiler () {
	local common_objects="vm.c common_ports.c sv.c"
	local invocation="cc -Wall -Wextra -Werror -pedantic -std=c99 $common_objects $1.c -o $1"
	echo -ne "$1\t"
	echo "$invocation"
	$invocation
}

if [ -z "$1" ]; then
	echo "Usage: make-tool.sh tools..."
	echo -e "\tTools are:"
	echo -e "\t\tassemble"
	echo -e "\t\tdisassemble"
	echo -e "\t\trun"
	echo -e "\t\tstepper"
	exit 1
fi

while [ ! -z "$1" ]; do
	case "$1" in
		"assemble")    run_compiler "assemble"    ;;
		"run")         run_compiler "run"         ;;
		"stepper")     run_compiler "stepper"     ;;
		"disassemble") run_compiler "disassemble" ;;
		*)
			echo "Unknown tool $1"
			exit 1
			;;
	esac
	shift
done
