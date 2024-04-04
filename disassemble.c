#include <stdbool.h>
#include <stdio.h>
#include "vm.h"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: disassemble <program.asm>\n");
		return 1;
	}

	char const *file_name = argv[1];
	FILE *file = fopen(file_name, "r");
	if (!file) {
		fprintf(stderr, "Could not open file.\n");
		return 1;
	}

	unsigned offset = 0;
	while (true) {
		uint8_t buf[3];
		size_t result = fread(buf, 1, sizeof buf, file);
		if (feof(file)) break;
		if (ferror(file)) {
			fprintf(stderr, "Error reading file\n");
			fclose(file);
			return 1;
		}
		if (result < 3) {
			fprintf(stderr, "File is not the correct size (should be a multiple of 3)\n");
			fclose(file);
			return 1;
		}

		printf(" %04x|\t%02x %02x %02x\t%s\n", offset, buf[0], buf[1], buf[2], vm_disasm(buf[0], buf[1], buf[2]));

		offset += 3;
	}

	fclose(file);
}
