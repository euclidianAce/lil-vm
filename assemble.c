#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
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
	exit(1); \
} while (0)

static char file_buf[4096];
sv read_whole_file(char const *path) {
	FILE *file = fopen(path, "r");
	if (fseek(file, 0, SEEK_END) < 0) goto fail_and_exit;
	long const count = ftell(file);
	if (count < 0) goto fail_and_exit;
	if ((size_t)count > sizeof file_buf) {
		fclose(file);
		fatal("File \"%s\" is too large (a max of %zu bytes is permitted)\n", path, sizeof file_buf);
	}

	if (fseek(file, 0, SEEK_SET) < 0) goto fail_and_exit;

	size_t read_count = 0;
	while (read_count < (size_t)count) {
		size_t result = fread(file_buf, 1, sizeof file_buf - read_count, file);
		if (result == 0) {
			if (feof(file)) break;
			if (ferror(file)) {
				fclose(file);
				fatal("An unknown error occurred when reading \"%s\"\n", path);
			}
		}
		read_count += result;
	}

	return (sv){ read_count, file_buf };

fail_and_exit: {}
	int e = errno;
	fclose(file);
	fatal("Could not read file \"%s\": %s (error %d)\n", path, strerror(e), e);
}

typedef enum asm_token_kind {
	asm_token_eof,
	asm_token_dot,
	asm_token_instruction,
	asm_token_register_name,
	asm_token_decimal,
	asm_token_hex,
	asm_token_rel_label_ref,
	asm_token_abs_label_ref,
	asm_token_label_def,
	asm_token_position,
} asm_token_kind;

typedef struct asm_token {
	asm_token_kind kind;
	union {
		uint32_t uint;
		int32_t sint;
		sv str;
		struct {
			vm_op opcode;
			vm_operands encoding;
		} instr;
	} u;
} asm_token;

static inline bool dec_digit(char c) {
	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return true;
	}
	return false;

}

static inline bool hex_digit(char c) {
	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return true;
	}
	return false;
}

static inline uint8_t dec_digit_value(char c) {
	assert(dec_digit(c));
	return c - '0';
}

static inline uint8_t hex_digit_value(char c) {
	assert(hex_digit(c));
	if ('a' <= c && c <= 'f')
		return 10 + c - 'a';
	if ('A' <= c && c <= 'F')
		return 10 + c - 'A';
	return dec_digit_value(c);
}

asm_token next_token(sv *s) {
	sv_chop_whitespace(s);

	while (sv_first(*s) == ';') {
		while (s->len > 0 && sv_first(*s) != '\n') {
			sv_chop_one(s);
		}
		sv_chop_whitespace(s);
	}

	if (s->len == 0)
		return (asm_token){ asm_token_eof, { 0 } };

	if (sv_starts_with(*s, sv_c("abs@"))) {
		sv_chop(s, 4);
		sv name = sv_chop_non_whitespace(s);
		return (asm_token){ asm_token_abs_label_ref, { .str = name } };
	}

	if (sv_starts_with(*s, sv_c("rel@"))) {
		sv_chop(s, 4);
		sv name = sv_chop_non_whitespace(s);
		return (asm_token){ asm_token_rel_label_ref, { .str = name } };
	}

	sv word = sv_chop_non_whitespace(s);

	if (sv_eq(word, sv_c("."))) return (asm_token){ asm_token_dot, { 0 } };

	if (sv_first(word) == '@' && sv_last(word) == ':') {
		if (word.len == 2) fatal("Invalid label definition \"" sv_fstr "\"", sv_farg(word));
		sv_chop(&word, 1);
		sv_chop_end(&word, 1);
		return (asm_token){ asm_token_label_def, { .str = word } };
	}

	if (sv_first(word) == '>') {
		sv full = word;
		sv_chop_one(&word);
		if (word.len != 4) { fatal("Invalid position \"" sv_fstr "\" (should have exactly 4 hex digits)", sv_farg(full)); }
		for (uint32_t i = 0; i < word.len; ++i)
			if (!hex_digit(word.data[i]))
				fatal("Invalid digit '%c' in position \"" sv_fstr "\"", word.data[i], sv_farg(full));

		uint16_t result = 0;
		for (uint8_t i = 0; i < word.len; ++i) {
			result *= 16;
			result += hex_digit_value(word.data[i]);
		}
		return (asm_token){ asm_token_position, { .uint = result } };
	}

	if (sv_first(word) == '#') {
		sv full = word;
		if (full.len != 3) { fatal("Invalid hex literal \"" sv_fstr "\" (should have exactly 2 digits)", sv_farg(full)); }
		sv_chop(&word, 1);

		for (uint32_t i = 0; i < word.len; ++i)
			if (!hex_digit(word.data[i]))
				fatal("Invalid digit '%c' in hex literal \"" sv_fstr "\"", word.data[i], sv_farg(full));

		return (asm_token){ asm_token_hex, { .uint = (hex_digit_value(word.data[0]) << 4) | hex_digit_value(word.data[1]) } };
	}

	if (dec_digit(sv_first(word))) {
		uint32_t result = 0;
		bool overflow = false;
		for (uint32_t i = 0; i < word.len; ++i) {
			char digit = word.data[i];

			if (!dec_digit(digit))
				fatal("Invalid decimal digit '%c' in \"" sv_fstr "\"", digit, sv_farg(word));

			result *= 10;
			result += dec_digit_value(digit);

			if (result > 0xffff)
				overflow = true;
		}
		if (overflow)
			fatal("Decimal number " sv_fstr " too large, (max of 65535 or 0xffff)", sv_farg(word));
		return (asm_token){ asm_token_decimal, { .uint = result } };
	}

	// Is this dumb? yes.
	// Does it work? also yes.
	if (sv_eq(word, sv_c("r0"))) return (asm_token){ asm_token_register_name, { .uint = 0 } };
	if (sv_eq(word, sv_c("r1"))) return (asm_token){ asm_token_register_name, { .uint = 1 } };
	if (sv_eq(word, sv_c("r2"))) return (asm_token){ asm_token_register_name, { .uint = 2 } };
	if (sv_eq(word, sv_c("r3"))) return (asm_token){ asm_token_register_name, { .uint = 3 } };
	if (sv_eq(word, sv_c("r4"))) return (asm_token){ asm_token_register_name, { .uint = 4 } };
	if (sv_eq(word, sv_c("r5"))) return (asm_token){ asm_token_register_name, { .uint = 5 } };
	if (sv_eq(word, sv_c("r6"))) return (asm_token){ asm_token_register_name, { .uint = 6 } };
	if (sv_eq(word, sv_c("r7"))) return (asm_token){ asm_token_register_name, { .uint = 7 } };
	if (sv_eq(word, sv_c("r8"))) return (asm_token){ asm_token_register_name, { .uint = 8 } };
	if (sv_eq(word, sv_c("r9"))) return (asm_token){ asm_token_register_name, { .uint = 9 } };
	if (sv_eq(word, sv_c("r10"))) return (asm_token){ asm_token_register_name, { .uint = 10 } };
	if (sv_eq(word, sv_c("r11"))) return (asm_token){ asm_token_register_name, { .uint = 11 } };
	if (sv_eq(word, sv_c("r12"))) return (asm_token){ asm_token_register_name, { .uint = 12 } };
	if (sv_eq(word, sv_c("r13"))) return (asm_token){ asm_token_register_name, { .uint = 13 } };
	if (sv_eq(word, sv_c("r14"))) return (asm_token){ asm_token_register_name, { .uint = 14 } };
	if (sv_eq(word, sv_c("r15"))) return (asm_token){ asm_token_register_name, { .uint = 15 } };

#define X(id, mnemonic, enc) \
	if (sv_eq(word, sv_c(mnemonic))) \
		return (asm_token){ asm_token_instruction, { .instr = { .opcode = vm_op_##id, .encoding = vm_operands_##enc } } };
	vm_x_instructions(X)
#undef X

	fatal("Invalid token " sv_fstr, sv_farg(word));
}

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

static inline uint8_t *last_byte_written(void) { return out_cursor - 1; }

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

asm_label *find_label(sv name) {
	for (size_t i = 0, c = static_buf_count(labels); i < c; ++i) {
		if (sv_eq(name, labels[i].name))
			return &labels[i];
	}
	return NULL;
}

asm_label *add_label(sv name) {
	if (static_buf_count(labels) >= static_buf_max_count(labels))
		fatal("Too many labels defined");
	if (find_label(name))
		fatal("Redefinition of label \"" sv_fstr "\"", sv_farg(name));
	asm_label *result = static_buf_add(labels);
	result->name = copy_sv(name);
	result->offset = current_offset();
	return result;
}

typedef struct asm_patch {
	sv name;
	bool absolute;
	uint16_t offset_to_be_relative_to;
	uint16_t offset_to_patch;
} asm_patch;

static_buf(asm_patch, patches, 1024);

typedef enum operand {
	operand_none,
	operand_register,
	operand_byte,
	operand_double_byte,
} operand;

sv operand_name(operand o) {
	switch (o) {
	case operand_none: return sv_c("nothing");
	case operand_register: return sv_c("register");
	case operand_byte: return sv_c("byte");
	case operand_double_byte: return sv_c("double byte");
	}

	return sv_c("???");
}

void assemble(sv contents) {
	vm_operands current_encoding = vm_operands_none;
	uint8_t expected_operand_index = 0;
	static operand const encoding_table[9][4] = {
		[vm_operands_none] = { operand_none,        operand_none,     operand_none,     operand_none },
		[vm_operands_rrrr] = { operand_register,    operand_register, operand_register, operand_register },
		[vm_operands_rrr]  = { operand_register,    operand_register, operand_register, operand_none },
		[vm_operands_rr]   = { operand_register,    operand_register, operand_none,     operand_none },
		[vm_operands_r]    = { operand_register,    operand_none,     operand_none,     operand_none },
		[vm_operands_rrb]  = { operand_register,    operand_register, operand_byte,     operand_none },
		[vm_operands_rb]   = { operand_register,    operand_byte,     operand_none,     operand_none },
		[vm_operands_bb]   = { operand_byte,        operand_byte,     operand_none,     operand_none },
		[vm_operands_d]    = { operand_double_byte, operand_none,     operand_none,     operand_none },
	};

#define expected_operand encoding_table[current_encoding][expected_operand_index]

	enum {
		state_any,
		state_expect_operand,
	} state = state_any;

	for (;;) {
		asm_token tk = next_token(&contents);

		switch (tk.kind) {
		case asm_token_eof:
			if (state != state_any)
				fatal("Unexpected end of file");
			return;

		case asm_token_instruction:
			if (state != state_any) fatal("Unexpected instruction name");
			current_encoding = tk.u.instr.encoding;
			expected_operand_index = 0;
			state = state_expect_operand;

			write_byte(tk.u.instr.opcode);
			continue;

		case asm_token_position:
			if (state != state_any) fatal("Unexpected position");
			out_cursor = out_buf + tk.u.uint;
			break;

		case asm_token_dot: {
			uint16_t to_write = current_aligned_offset();
			switch (expected_operand) {
			case operand_byte:
				if (to_write > 255)
					fatal("Expected a byte, but current offset (.) is too large (%u)", to_write);
				write_byte((uint8_t)to_write);
				break;
			case operand_double_byte:
				write_byte(to_write >> 8);
				write_byte((uint8_t)to_write);
				break;
			default: {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected ., expected " sv_fstr, sv_farg(expected_name));
			}
			}
			break;
		}

		case asm_token_register_name:
			if (expected_operand != operand_register) {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected register name, expected " sv_fstr, sv_farg(expected_name));
			}
			if (expected_operand_index % 2 == 0) {
				write_byte((uint8_t)tk.u.uint << 4);
			} else {
				*last_byte_written() |= (uint8_t)tk.u.uint;
			}
			break;

		case asm_token_decimal:
			switch (expected_operand) {
			case operand_byte:
				if (tk.u.uint > 255)
					fatal("Expected a byte, but %u is too large", tk.u.uint);
				write_byte((uint8_t)tk.u.uint);
				break;
			case operand_double_byte:
				write_byte((uint16_t)tk.u.uint >> 8);
				write_byte((uint8_t)tk.u.uint);
				break;
			default: {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected dec number, expected " sv_fstr, sv_farg(expected_name));
			}
			}
			break;

		case asm_token_hex:
			if (state == state_any) {
				write_byte((uint8_t)tk.u.uint);
				break;
			}

			switch (expected_operand) {
			case operand_byte:
				write_byte((uint8_t)tk.u.uint);
				break;
			case operand_double_byte:
				write_byte(0);
				write_byte((uint8_t)tk.u.uint);
				break;
			default: {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected hex number, expected " sv_fstr, sv_farg(expected_name));
			}
			}
			break;

		case asm_token_rel_label_ref: {
			// TODO: label refs should error about size at patch time
			if (expected_operand != operand_double_byte) {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected label ref, expected (" sv_fstr ")", sv_farg(expected_name));
			}
			asm_patch *patch = static_buf_add(patches);
			*patch = (asm_patch) {
				.absolute = false,
				.offset_to_be_relative_to = current_aligned_offset(),
				.name = tk.u.str,
				.offset_to_patch = current_offset(),
			};
			break;
		}

		case asm_token_abs_label_ref: {
			if (expected_operand != operand_double_byte) {
				sv expected_name = operand_name(expected_operand);
				fatal("Unexpected label ref, expected (" sv_fstr ")", sv_farg(expected_name));
			}
			asm_patch *patch = static_buf_add(patches);
			*patch = (asm_patch) {
				.absolute = true,
				.name = tk.u.str,
				.offset_to_patch = current_offset(),
			};
			break;
		}

		case asm_token_label_def: {
			if (state != state_any) fatal("Unexpected label definition");
			add_label(tk.u.str);
			break;
		}
		}

		if (state == state_expect_operand) {
			++expected_operand_index;
			if (expected_operand == operand_none) {
				state = state_any;
				pad();
			}
		}
	}
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
			out_buf[patch->offset_to_patch]     = ((uint16_t)a) >> 8;
			out_buf[patch->offset_to_patch + 1] = ((uint16_t)a) & 0xff;
		}
	}

	if (had_error)
		exit(1);
}

int main(int argc, char **argv) {
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: assemble <program.asm> [output]\n");
		return 1;
	}

	char const *file_name = argv[1];
	char const *out_file_name = argc == 3 ? argv[2] : "out";

	sv contents = read_whole_file(file_name);
	assemble(contents);
	apply_patches();

	size_t result_len = out_cursor - out_buf;
	FILE *output = fopen(out_file_name, "wbc");
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
