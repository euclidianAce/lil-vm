#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vm.h"
#include "sv.h"
#include "static_buf.h"

#define todo(...) do { \
	fprintf(stderr, "TODO " __FILE__ " line %d: ", __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	exit(9); \
} while (0)

#define fatal(...) do { \
	fprintf(stderr, "Fatal error: "); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	exit(10); \
} while (0)

static_buf(char, characters, 4096);
sv copy_sv(sv s) {
	size_t offset = static_buf_count(characters);
	static_buf_count(characters) += s.len;
	char *result = &characters[offset];
	memcpy(result, s.data, s.len);
	return (sv){ s.len, result };
}

static uint8_t out_buf[0xffff];
uint8_t *out_cursor = out_buf;
static inline void write_byte(uint8_t b) { *out_cursor++ = b; }
static inline void pad(void) {
	switch ((out_cursor - out_buf) % 3) {
	case 1: write_byte(0); write_byte(0); break;
	case 2: write_byte(0); break;
	case 0: break;
	}
}

static inline uint16_t current_offset(void) { return out_cursor - out_buf; }
static inline uint16_t current_aligned_offset(void) {
	uint16_t actual = current_offset();
	return actual - actual % 3;
}

typedef struct asm_label {
	sv name;
	uint16_t offset;
} asm_label;

static_buf(asm_label, labels, 512);

asm_label *add_label(sv name) {
	if (static_buf_count(labels) >= static_buf_max_count(labels))
		fatal("Too many labels defined");
	asm_label *result = static_buf_add(labels);
	result->name = copy_sv(name);
	result->offset = current_offset();
	return result;
}

asm_label *find_label(sv name) {
	for (size_t i = 0, c = static_buf_count(labels); i < c; ++i) {
		if (sv_eq(name, labels[i].name))
			return &labels[i];
	}
	return NULL;
}

typedef struct asm_patch {
	sv name;
	bool absolute;
	uint16_t offset_to_be_relative_to;
	uint16_t offset_to_patch;
} asm_patch;

static_buf(asm_patch, patches, 1024);

// trims leading whitespace
//
// returns NULL on eof
// exits on error
sv read_line(FILE *file) {
	static char buf[1024];
	static char *start = buf, *end = buf;

	if (start >= end)  {
		if (feof(file)) return (sv){ 0, NULL };

		start = buf;
		size_t result = fread(start, 1, sizeof buf, file);
		if (result == 0) {
			if (feof(file)) return (sv){ 0, NULL };
			if (ferror(file)) {
				fprintf(stderr, "Error reading file\n");
				fclose(file);
				exit(1);
			}
		}
		end = start + result;
	}

	char *cursor = start;
	while (cursor < end && *cursor != '\n')
		++cursor;

	while (start < cursor && (*start == ' ' || *start == '\t'))
		++start;

	static char ret_buf[1024];
	size_t len = cursor - start;
	++cursor;
	memcpy(ret_buf, start, len);
	start = cursor;

	return (sv){ len, ret_buf };
}

int8_t register_name(sv s) {
	// fprintf(stderr, "register_name(\"%.*s\")\n", (int)s.len, s.data);
	if (s.len == 0) return -1;
	if (s.data[0] != 'r')
		return -1;

	if (s.len == 2) {
		switch (s.data[1]) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return s.data[1] - '0';
		}
	} else if (s.len == 3) {
		if (s.data[1] != '1')
			return -1;
		switch (s.data[2]) {
		case '0': case '1': case '2':
		case '3': case '4': case '5':
			return s.data[2] - '0' + 10;
		}
	}

	return -1;
}

int32_t parse_integer(sv s) {
	if (sv_eq(s, sv_c(".")))
		return current_aligned_offset();

	if (sv_starts_with(s, sv_c("abs@"))) {
		asm_patch *patch = static_buf_add(patches);
		sv_chop(&s, 4);
		*patch = (asm_patch) {
			.absolute = true,
			.offset_to_be_relative_to = 0,
			.name = copy_sv(s),
			.offset_to_patch = current_offset(),
		};
		return 0xffff;
	}

	if (sv_starts_with(s, sv_c("rel@"))) {
		asm_patch *patch = static_buf_add(patches);
		sv_chop(&s, 4);
		*patch = (asm_patch) {
			.absolute = false,
			.offset_to_be_relative_to = current_aligned_offset(),
			.name = copy_sv(s),
			.offset_to_patch = current_offset(),
		};
		return 0xffff;
	}

	uint32_t result = 0;
	uint8_t base = 10;

	for (uint32_t i = 0; i < s.len; ++i) {
		switch (s.data[i]) {
		case '#':
			if (i == 0) {
				base = 16;
			} else {
				return -1;
			}
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			result *= base;
			result += s.data[i] - '0';
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			if (base == 16) {
				result *= base;
				result += s.data[i] - 'a' + 10;
			} else {
				return -1;
			}
			break;
		default:
			return -1;
		}

		if (result > 0xffff)
			return -1;
	}

	return (uint16_t)result;
}

void assemble_line(sv line) {
	if (line.len == 0) return;

	sv word = sv_chop_word(&line);

	if (sv_first(word) == '@') {
		if (sv_last(word) != ':') { todo("bad label name"); }
		if (word.len == 2) { todo("bad label name"); }

		sv name = word;
		sv_chop(&name, 1);
		sv_chop_end(&name, 1);

		add_label(name);

		word = sv_chop_word(&line);
	}

	if (word.len == 0) return;

	vm_op opcode;
	vm_operands encoding;

#define X(id, mnemonic, enc) if (sv_eq(word, sv_from_c(mnemonic))) { \
	opcode = vm_op_##id; \
	encoding = enc; \
	goto found; \
}
	OPERATIONS(X)
#undef X

	fprintf(stderr, "Unknown instruction \"%.*s\". (TODO: make this better)\n", (int)word.len, word.data);
	exit(1);

found: {}

	write_byte(opcode);

#define expect_reg(out_reg) do { \
	int8_t _reg_ = register_name(sv_chop_word(&line)); \
	if (_reg_ < 0) { todo("report error, expected register name"); } \
	out_reg = _reg_; \
} while (0)

#define expect_byte(out_byte) do { \
	int32_t _byte_ = parse_integer(sv_chop_word(&line)); \
	if (_byte_ < 0) { todo("report error, invalid digits"); } \
	if (_byte_ > 255) { todo("report error, byte too big"); } \
	out_byte = (uint8_t)_byte_; \
} while (0)

#define expect_double_byte(out_double_byte) do { \
	int32_t _double_byte_ = parse_integer(sv_chop_word(&line)); \
	if (_double_byte_ < 0) { todo("report error, invalid digits"); } \
	out_double_byte = (uint16_t)_double_byte_; \
} while (0)

#define expect_eol() do { sv word = sv_chop_word(&line); if (word.len > 0) { todo("report error, trailing characters"); } } while (0)

	switch (encoding) {
	case none: expect_eol(); break;
	case rrrr: {
		uint8_t reg_1 = 0; expect_reg(reg_1);
		uint8_t reg_2 = 0; expect_reg(reg_2);
		uint8_t reg_3 = 0; expect_reg(reg_3);
		uint8_t reg_4 = 0; expect_reg(reg_4);
		write_byte((reg_1 << 4) | reg_2);
		write_byte((reg_3 << 4) | reg_4);
		expect_eol();
		break;
	}
	case rrr: {
		uint8_t reg_1 = 0; expect_reg(reg_1);
		uint8_t reg_2 = 0; expect_reg(reg_2);
		uint8_t reg_3 = 0; expect_reg(reg_3);
		write_byte((reg_1 << 4) | reg_2);
		write_byte(reg_3 << 4);
		expect_eol();
		break;
	}
	case rr: {
		uint8_t reg_1 = 0; expect_reg(reg_1);
		uint8_t reg_2 = 0; expect_reg(reg_2);
		write_byte((reg_1 << 4) | reg_2);
		expect_eol();
		break;
	}
	case r: {
		uint8_t reg = 0; expect_reg(reg);
		write_byte(reg << 4);
		expect_eol();
		break;
	}
	case rb: {
		uint8_t reg = 0; expect_reg(reg);
		write_byte(reg << 4);
		uint8_t b = 0; expect_byte(b);
		write_byte(b);
		expect_eol();
		break;
	}
	case rrb: {
		uint8_t reg_1 = 0; expect_reg(reg_1);
		uint8_t reg_2 = 0; expect_reg(reg_2);
		write_byte((reg_1 << 4) | reg_2);
		uint8_t b = 0; expect_byte(b);
		write_byte(b);
		expect_eol();
		break;
	}
	case bb: {
		uint8_t b = 0;
		expect_byte(b); write_byte(b);
		expect_byte(b); write_byte(b);
		expect_eol();
		break;
	}
	case d: {
		 uint16_t db = 0;
		 expect_double_byte(db);
		 write_byte(db >> 8);
		 write_byte(db & 0xff);
		 expect_eol();
		 break;
	}
	}

	pad();
}

void apply_patches(void) {
	bool had_error = false;
	for (size_t i = 0, c = static_buf_count(patches); i < c; ++i) {
		asm_patch const *patch = &patches[i];
		asm_label const *label = find_label(patch->name);

		if (!label) {
			fprintf(
				stderr,
				"Unknown label \"%.*s\"\n",
				(int)patch->name.len,
				patch->name.data
			);
			had_error = true;
			continue;
		}

		if (patch->absolute) {
			out_buf[patch->offset_to_patch]     = ((uint16_t)label->offset) >> 8;
			out_buf[patch->offset_to_patch + 1] = ((uint16_t)label->offset) & 0xff;
		} else {
			int32_t a = label->offset;
			a -= patch->offset_to_be_relative_to;
			a -= 3;
			out_buf[patch->offset_to_patch]     = ((uint16_t)a) >> 8;
			out_buf[patch->offset_to_patch + 1] = ((uint16_t)a) & 0xff;
		}
	}

	if (had_error)
		exit(1);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: assemble <program.asm>\n");
		return 1;
	}

	char const *file_name = argv[1];
	FILE *file = fopen(file_name, "r");
	if (!file) {
		fprintf(stderr, "Could not open file.\n");
		return 1;
	}

	sv line;
	while ((line = read_line(file)).data)
		assemble_line(line);

	fclose(file);

	apply_patches();

	char const *out_file_name = "out";

	size_t result_len = out_cursor - out_buf;
	FILE *output = fopen(out_file_name, "wc");
	{
		size_t result = fwrite(out_buf, 1, result_len, output);
		if (result != result_len) {
			fprintf(stderr, "Could not write assembled output\n.");
			exit(1);
		}
	}
	fclose(output);

	printf("Wrote %zu bytes to %s\n", result_len, out_file_name);
}
