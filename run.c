#include "vm.h"
#include "common_ports.h"
#include <stdio.h>
#include <stdlib.h>

#include "vm_utils.c"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: run <program>\n");
		return 1;
	}
	char const *program = argv[1];

	vm_state vm;
	vm_init(&vm);
	initialize_extra(&vm);

	read_file_to_vm_memory(&vm, program);

	for (int i = 0; i < 10000; ++i)
		vm_step(&vm);
}
