#include "vm.h"
#include "common_ports.h"
#include "sv.h"

#define _XOPEN_SOURCE 1

#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <threads.h>

#include "vm_utils.c"

static void usage(void) {
	fprintf(stderr, "Usage: run [-c cores=1] [-t threads=1] <program>\n");
}

typedef struct thread_data {
	uint8_t first_core, core_count; // the range of cores this thread is responsible for driving
	uint64_t rng_state;
} thread_data;
int thread_func(void *);
static vm_core core_storage[256];
static common_port_state state;
static vm_state vm;

int main(int argc, char **argv) {
	char const *file_name = "";
	int core_count = 1;
	int thread_count = 1;

	int opt;
	while ((opt = getopt(argc, argv, "c:t:")) != -1) switch (opt) {
	case 'c': core_count = atoi(optarg); break;
	case 't': thread_count = atoi(optarg); break;
	}

	if (core_count <= 0 || core_count >= 256) {
		usage();
		fprintf(stderr, "Invalid core count given.\n");
		return 1;
	}
	if (thread_count <= 0 || thread_count >= 256) {
		usage();
		fprintf(stderr, "Invalid thread count given.\n");
		return 1;
	}
	if (thread_count > core_count) {
		fprintf(stderr, "Thread count can't be greater than core count\n");
		return 1;
	}

	if (optind == argc) {
		usage();
		fprintf(stderr, "No file name given\n");
		return 1;
	}

	file_name = argv[optind];

	vm_init(&vm, core_count, core_storage);
	vm_install_common_ports(&vm, &state);
	read_file_to_vm_memory(&vm, file_name);

	srand(time(0));

	if (thread_count == 1) {
		// don't bother spawning threads if we just need one
		return thread_func(&(thread_data){
			.first_core = 0,
			.core_count = core_count,
			.rng_state = rand(),
		});
	}

	uint8_t cores_per_thread = core_count / thread_count;
	uint8_t remainder = core_count % thread_count;

	static thrd_t threads[256];
	static thread_data thread_data_storage[256];
	for (uint8_t i = 0; i < thread_count; ++i) {
		thread_data_storage[i] = (thread_data){
			.first_core = cores_per_thread * i,
			.core_count = cores_per_thread,
			.rng_state = rand(),
		};
		if (i == thread_count - 1)
			thread_data_storage[i].core_count += remainder;
		if (thrd_create(&threads[i], thread_func, &thread_data_storage[i]) != thrd_success) {
			fprintf(stderr, "Failed to spawn a thread\n");
			return 1;
		}
	}

	for (uint8_t i = 0; i < thread_count; ++i)
		thrd_join(threads[i], NULL);
}

uint64_t rng_next(uint64_t *state) {
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	return *state = x;
}

int thread_func(void *data_) {
	thread_data *data = data_;

	for (; !state.wrote_to_shut_down;) {
		uint8_t core_index = data->first_core + rng_next(&data->rng_state) % data->core_count;
		vm_step(&vm, core_index);

		if (vm.cores[core_index].fault != vm_fault_none) {
			printf(
				"Machine core %u faulted with fault %u (%s) at pc=%04x.\n",
				core_index,
				vm.cores[core_index].fault,
				vm_fault_name(vm.cores[core_index].fault),
				vm.cores[core_index].pc
			);
			return vm.cores[core_index].fault;
		}
	}

	return 0;
}
