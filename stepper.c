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

	{
		bool diff = now->core.pc != prev->core.pc;
		if (diff) printf("\033[33m");
		printf("  pc:%04x", now->core.pc);
		if (diff) printf("\033[0m");
	}

	{
		bool diff = now->core.fault != prev->core.fault;
		if (diff) printf("\033[33m");
		printf("  fault:%x\n", now->core.fault);
		if (diff) printf("\033[0m");
	}

	// always show registers, but highlight diffs

	for (uint8_t i = 0; i < 16; ++i) {
		bool diff = prev->core.registers[i] != now->core.registers[i];
		if (diff) printf("\033[33m");
		printf(" ");
		printf("%s:%04x", vm_padded_reg_name(i), now->core.registers[i]);
		if (diff) printf("\033[0m");
		if (i == 7) printf("\n");
	}

	printf("\n");

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

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: stepper <program.asm>\n");
		return 1;
	}

	char const *file_name = argv[1];

	vm_state prev, now;
	vm_init(&now);
	initialize_extra(&now);

	read_file_to_vm_memory(&now, file_name);

	memcpy(&prev, &now, sizeof now);
	show_delta(&prev, &now);

	for (;;) {
		printf(
			"\nNext instruction: \033[1m%s\033[0m (raw %02x %02x %02x)\nPress enter to continue, Control+C to quit.\n",
			vm_disasm_pc(&now),
			now.memory[now.core.pc],
			now.memory[now.core.pc + 1],
			now.memory[now.core.pc + 2]
		);
		getchar();
		vm_step(&now);
		show_delta(&prev, &now);
		memcpy(&prev, &now, sizeof now);
	}
}
