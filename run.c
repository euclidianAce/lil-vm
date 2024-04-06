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

	common_port_state state;
	vm_state vm;
	vm_init(&vm);

	read_file_to_vm_memory(&vm, program);

	for (; !state.wrote_to_shut_down && vm.core.fault == vm_fault_none;)
		vm_step(&vm);

	if (vm.core.fault != vm_fault_none) {
		printf("Machine faulted with fault %u (%s) at pc=%04x.\n", vm.core.fault, vm_fault_name(vm.core.fault), vm.core.pc);
		return vm.core.fault;
	}
}
