#include <stdlib.h>

// installs common ports and sets up stack pointer
void initialize_extra(vm_state *vm) {
	vm_install_common_ports(vm);
	vm->core.registers[15] = 0xf000;
}

void read_file_to_vm_memory(vm_state *vm, char const *path) {
	FILE *file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "Could not open file %s.\n", path);
		exit(1);
	}

	for (uint8_t *cursor = &vm->memory[0];;) {
		size_t remaining = &vm->memory[0] + sizeof(vm->memory) - cursor;
		if (remaining == 0 && !feof(file)) {
			fprintf(stderr, "This file is too large (should be less than 65536 bytes).\n");
			fclose(file);
			exit(1);
		}

		size_t result = fread(cursor, 1, remaining, file);
		if (result == 0) {
			if (feof(file)) break;

			if (ferror(file)) {
				fprintf(stderr, "There was an error reading the file.\n");
				fclose(file);
				exit(1);
			}
		}
		cursor += result;
	}
	fclose(file);
}
