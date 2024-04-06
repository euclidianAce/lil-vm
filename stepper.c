#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "vm.h"
#include "mnemonics.h"
#include "common_ports.h"

static void show_delta(
	vm_state const *prev,
	vm_state const *now
) {
	for (uint16_t i = 0; i < now->core_count; ++i) {
		if (i > 0) printf("\n");

		vm_core const *prev_core = &prev->cores[i];
		vm_core const *now_core = &now->cores[i];

		printf("  id:%04x", i);

		{
			bool diff = now_core->pc != prev_core->pc;
			if (diff) printf("\033[33m");
			printf("  pc:%04x", now_core->pc);
			if (diff) printf("\033[0m");
		}

		{
			bool diff = now_core->fault != prev_core->fault;
			if (diff) printf("\033[33m");
			printf("  fault:%x\n", now_core->fault);
			if (diff) printf("\033[0m");
		}

		// always show registers, but highlight diffs
		for (uint8_t i = 0; i < 16; ++i) {
			bool diff = prev_core->registers[i] != now_core->registers[i];
			if (diff) printf("\033[33m");
			printf(" ");
			printf("%s:%04x", vm_padded_reg_name(i), now_core->registers[i]);
			if (diff) printf("\033[0m");
			if (i == 7) printf("\n");
		}

		printf("\n");
	}

	bool first_mem_diff = true;

	for (uint32_t i = 0; i < UINT16_MAX; i += 8) {
		if (memcmp(&prev->memory[i], &now->memory[i], 8) == 0)
			continue;

		if (first_mem_diff) {
			first_mem_diff = false;
			printf("\n");
		}

		printf(" %04x|", i);
		for (uint32_t j = i; j < i + 8; ++j) {
			bool diff = prev->memory[j] != now->memory[j];
			if (diff) printf("\033[33m");
			printf(" %02x", now->memory[j]);
			if (diff) printf("\033[0m");
		}

		printf("\n");
	}
}

#include "vm_utils.c"

void copy_vm_state(vm_state *to, vm_state const *from) {
	assert(from->core_count == to->core_count);
	memcpy(to->cores, from->cores, from->core_count * sizeof(vm_core));
	memcpy(to->memory, from->memory, sizeof from->memory);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: stepper <program.asm>\n");
		return 1;
	}

	char const *file_name = argv[1];

#define STATIC_CORE_COUNT 3

	common_port_state state;
	vm_state prev, now;
	vm_core prev_cores[STATIC_CORE_COUNT];
	vm_core now_cores[STATIC_CORE_COUNT];
	vm_init(&now, STATIC_CORE_COUNT, now_cores);
	prev.cores = prev_cores;
	prev.core_count = STATIC_CORE_COUNT;

	vm_install_common_ports(&prev, &state);
	vm_install_common_ports(&now, &state);

	read_file_to_vm_memory(&now, file_name);

	copy_vm_state(&prev, &now);

	show_delta(&prev, &now);

	for (uint16_t core_index = 0; !state.wrote_to_shut_down;) {
		printf(
			"\nCore %u, Next instruction: \033[1m%s\033[0m (raw %02x %02x %02x)\nPress enter to continue, Control+C to quit.\n",
			core_index,
			vm_disasm_pc(&now, core_index),
			now.memory[now.cores[core_index].pc],
			now.memory[now.cores[core_index].pc + 1],
			now.memory[now.cores[core_index].pc + 2]
		);
		getchar();

		vm_step(&now, core_index);
		show_delta(&prev, &now);
		copy_vm_state(&prev, &now);

		if (now.cores[core_index].fault != vm_fault_none) {
			printf("Machine core %u faulted with fault %u (%s) at pc=%04x.\n", core_index, now.cores[core_index].fault, vm_fault_name(now.cores[core_index].fault), now.cores[core_index].pc);
			return now.cores[core_index].fault;
		}

		core_index += 1;
		core_index %= STATIC_CORE_COUNT;
	}

	if (state.wrote_to_shut_down)
		printf("Machine requested shut down.\n");
}
