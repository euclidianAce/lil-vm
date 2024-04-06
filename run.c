#include "vm.h"
#include "common_ports.h"
#include "sv.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <threads.h>

#include "vm_utils.c"

static void usage(void) {
	fprintf(stderr, "Usage: run [-c cores=1] <program>\n");
}

int thread_func(void *);
static vm_core core_storage[256];
static common_port_state state;
static vm_state vm;

int main(int argc, char **argv) {
	char const *file_name = "";
	int core_count = 1;

	switch (argc) {
	default: usage(); return 1;

	case 2: {
		sv arg = sv_from_c(argv[1]);
		if (sv_starts_with(arg, sv_c("-c"))) { usage(); return 1; }
		file_name = argv[1];
		break;
	}
	case 3: {
		sv arg = sv_from_c(argv[1]);
		if (!sv_starts_with(arg, sv_c("-c"))) { usage(); return 1; }
		sv_chop(&arg, 2);
		core_count = atoi(arg.data);
		file_name = argv[2];
		break;
	}

	case 4: {
		if (!sv_eq(sv_c("-c"), sv_from_c(argv[1]))) { usage(); return 1; }
		core_count = atoi(argv[2]);
		file_name = argv[3];
		break;
	}
	}

	if (core_count <= 0 || core_count >= 256) {
		usage();
		fprintf(stderr, "Invalid core count given.");
		return 1;
	}

	vm_init(&vm, core_count, core_storage);
	vm_install_common_ports(&vm, &state);
	read_file_to_vm_memory(&vm, file_name);

	static thrd_t threads[256];
	for (uint8_t i = 0; i < core_count; ++i) {
		if (thrd_create(&threads[i], thread_func, (void *)(uintptr_t)i) != thrd_success) {
			fprintf(stderr, "Failed to spawn a thread\n");
			return 1;
		}
	}

	for (uint8_t i = 0; i < core_count; ++i)
		thrd_join(threads[i], NULL);
}

int thread_func(void *index_as_p) {
	uint8_t const core_index = (uint8_t)(uintptr_t)index_as_p;

	for (; !state.wrote_to_shut_down;) {
		vm_step(&vm, core_index);

		if (vm.cores[core_index].fault != vm_fault_none) {
			printf("Machine core %u faulted with fault %u (%s) at pc=%04x.\n", core_index, vm.cores[core_index].fault, vm_fault_name(vm.cores[core_index].fault), vm.cores[core_index].pc);
			return vm.cores[core_index].fault;
		}
	}

	return 0;
}
