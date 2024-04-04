#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "ops.h"
#include "vm.h"

static void vm_push(vm_state *vm, uint16_t value) {
	vm->core.registers[15] += 2;
	uint16_t cursor = vm->core.registers[15]; 
	vm->memory[cursor + 1] = value >> 8;
	vm->memory[cursor] = value & 0xff;
}

static uint16_t vm_pop(vm_state *vm) {
	uint16_t cursor = vm->core.registers[15];
	uint16_t result = (vm->memory[cursor + 1] << 8) | vm->memory[cursor];
	vm->core.registers[15] -= 2;

	return result;
}

void vm_init(vm_state *vm) {
	vm->core.pc = 0;
	vm->ports = (vm_ports){
		.context = NULL,
		.port_read = NULL,
		.port_write = NULL,
	};
	for (uint16_t i = 0; i < sizeof vm->memory; ++i)
		vm->memory[i] = 0;
}

char const *op_name(uint8_t code) {
#define X(name, mnemonic, encoding) case vm_op_##name : return #name;
	switch (code) { OPERATIONS(X) }
#undef X
	return "???";
}

char const *op_mnemonic(uint8_t code) {
#define X(name, mnemonic, encoding) case vm_op_##name : return mnemonic;
	switch (code) { OPERATIONS(X) }
#undef X
	return "???";
}

vm_operands op_encoding(uint8_t code) {
#define X(name, mnemonic, encoding) case vm_op_##name : return encoding;
	switch (code) { OPERATIONS(X) }
#undef X
	return -1;
}

#define unimplemented() do { fprintf(stderr, "Unimplemented opcode %s\n", vm_disasm(op, b, c)); abort(); } while (0)

#define R1_id (b >> 4)
#define R2_id (b & 0x0f)
#define R3_id (c >> 4)
#define R4_id (c & 0x0f)

#define R1 (&vm->core.registers[R1_id])
#define R2 (&vm->core.registers[R2_id])
#define R3 (&vm->core.registers[R3_id])
#define R4 (&vm->core.registers[R4_id])

#define SR1 ((int8_t *)R1)
#define SR2 ((int8_t *)R2)
#define SR3 ((int8_t *)R3)
#define SR4 ((int8_t *)R4)

#define B1 (b)
#define B2 (c)

#define D ((b << 8) | c)
#define SD ((int16_t)((b << 8) | c))

char const *vm_disasm(uint8_t op, uint8_t b, uint8_t c) {
	static char buf[128];

	char *ptr = buf;
#define addf(...) do { ptr += snprintf(ptr, buf + sizeof(buf) - ptr, "" __VA_ARGS__); } while (0)

	addf("%s ", op_mnemonic(op));

#define reg_fmt "r%d"
#define byte_fmt "%02x"
#define two_byte_fmt "%04x"

	switch (op_encoding(op)) {
	case none: break;
	case rrrr: addf(reg_fmt " " reg_fmt " " reg_fmt " " reg_fmt, R1_id, R2_id, R3_id, R4_id); break;
	case rrr: addf(reg_fmt " " reg_fmt " " reg_fmt, R1_id, R2_id, R3_id); break;
	case rrb: addf(reg_fmt " " reg_fmt " " byte_fmt, R1_id, R2_id, B2); break;
	case rb: addf(reg_fmt " " byte_fmt, R1_id, B2); break;
	case rr: addf(reg_fmt " " reg_fmt, R1_id, R2_id); break;
	case bb: addf(byte_fmt " " byte_fmt, B1, B2); break;
	case r: addf(reg_fmt, R1_id); break;
	case d: addf(two_byte_fmt, D); break;
	}

#undef addf

	return buf;
}

char const *vm_disasm_pc(vm_state const *vm) {
	return vm_disasm(vm->memory[vm->core.pc], vm->memory[vm->core.pc + 1], vm->memory[vm->core.pc + 2]);
}

char const *vm_padded_reg_name(uint8_t n) {
	switch (n) {
	case 0: return " r0";
	case 1: return " r1";
	case 2: return " r2";
	case 3: return " r3";
	case 4: return " r4";
	case 5: return " r5";
	case 6: return " r6";
	case 7: return " r7";
	case 8: return " r8";
	case 9: return " r9";
	case 10: return "r10";
	case 11: return "r11";
	case 12: return "r12";
	case 13: return "r13";
	case 14: return "r14";
	case 15: return "r15";
	}

	return "???";
}


void vm_step(vm_state *vm) {
	uint8_t const op = vm->memory[vm->core.pc++];
	uint8_t const b = vm->memory[vm->core.pc++];
	uint8_t const c = vm->memory[vm->core.pc++];

	switch (op) {
	case vm_op_Nop: break;

	case vm_op_Load_Immediate_Byte:     *R1 = B2;               break;
	case vm_op_Shift_In_Byte:           *R1 = (*R1 << 8) | B2;  break;
	case vm_op_Shift_Left:              *R1 <<= *R2;            break;
	case vm_op_Shift_Logical_Right:     *R1 >>= *R2;            break;
	case vm_op_Shift_Arithmetic_Right:  *SR1 >>= *R2;           break;

	case vm_op_Copy: *R1 = *R2; break;

	case vm_op_Rotate_2: {
		uint16_t v1 = *R1, v2 = *R2;
		*R1 = v2;
		*R2 = v1;
		break;
	}

	case vm_op_Rotate_3: {
		uint16_t v1 = *R1, v2 = *R2, v3 = *R3;
		*R1 = v2;
		*R2 = v3;
		*R3 = v1;
		break;
	}

	case vm_op_Rotate_4: {
		uint16_t v1 = *R1, v2 = *R2, v3 = *R3, v4 = *R4;
		*R1 = v2;
		*R2 = v3;
		*R3 = v4;
		*R4 = v1;
		break;
	}

	case vm_op_Add: {
		uint32_t result = *R3 + *R4;
		*R1 = result >> 16;
		*R2 = result & 0xffff;
		break;
	}

	case vm_op_Increment: {
		uint32_t result = *R2 + B2;
		*R1 = result >> 16;
		*R2 = result & 0xffff;
		break;
	}

	case vm_op_Decrement: {
		uint32_t result = *R2 - B2;
		*R1 = result >> 16;
		*R2 = result & 0xffff;
		break;
	}

	case vm_op_Subtract: {
		uint32_t result = *R3 - *R4;
		*R1 = result >> 16;
		*R2 = result & 0xffff;
		break;
	}

	case vm_op_Unsigned_Multiply: {
		uint32_t result = *R3 * *R4;
		*R1 = result >> 16;
		*R2 = result & 0xffff;
		break;
	}

	case vm_op_Signed_Multiply: {
		union {
			int32_t s;
			uint32_t u;
		} result = { .s = *SR3 * *SR4 };
		*R1 = result.u >> 16;
		*R2 = result.u & 0xffff;
		break;
	}

	case vm_op_Unsigned_Divide: {
		*R1 = *R3 / *R4;
		*R2 = *R3 % *R4;
		break;
	}

	case vm_op_Signed_Divide: {
		union {
			int16_t s;
			uint16_t u;
		} div = { .s = *SR3 / *SR4 }, rem = { .s = *SR3 % *SR4 };
		*R1 = div.u;
		*R2 = rem.u;
		break;
	}

	case vm_op_Compare_Signed: {
		bool cmp = *SR3 <= *SR4;
		*R1 = cmp;
		*R2 = !cmp;
		break;
	}
	case vm_op_Compare_Unsigned: {
		bool cmp = *R3 <= *R4;
		*R1 = cmp;
		*R2 = !cmp;
		break;
	}
	case vm_op_Compare_Equal: {
		bool cmp = *R3 == *R4;
		*R1 = cmp;
		*R2 = !cmp;
		break;
	}

	case vm_op_Branch_Immediate_Absolute: vm->core.pc = D; break;
	case vm_op_Branch_Immediate_Relative: vm->core.pc += SD; break;
	case vm_op_Branch_Absolute: vm->core.pc = *R1; break;
	case vm_op_Branch_Relative: vm->core.pc += *SR1; break;

	case vm_op_Skip_If_Zero:     if (*R1 == 0) vm->core.pc += 3; break;
	case vm_op_Skip_If_Non_Zero: if (*R1 != 0) vm->core.pc += 3; break;

	case vm_op_Read_Address_Byte:      *R1 = vm->memory[*R2]; break;
	case vm_op_Read_Address_Two_Byte:  *R1 = (vm->memory[*R2 + 1] << 8) | vm->memory[*R2]; break;
	case vm_op_Write_Address_Byte:     vm->memory[*R1] = *R2; break;
	case vm_op_Write_Address_Two_Byte: vm->memory[*R1] = *R2; vm->memory[*R1 + 1] = *R2 >> 8; break;

	case vm_op_Push: vm_push(vm, *R1); break;
	case vm_op_Pop: *R1 = vm_pop(vm); break;

	case vm_op_Port_Write:
		if (vm->ports.port_write)
			vm->ports.port_write(vm->ports.context, B2, *R1);
		// TODO: else fault
		break;
	case vm_op_Port_Read:
		if (vm->ports.port_read)
			*R1 = vm->ports.port_read(vm->ports.context, B2);
		// TODO: else fault
		break;

	case vm_op_Call_Immediate_Relative: {
		uint16_t ret_addr = vm->core.pc;
		vm_push(vm, ret_addr);
		vm->core.pc += D;
		break;
	}

	case vm_op_Call_Immediate_Absolute: {
		uint16_t ret_addr = vm->core.pc;
		vm_push(vm, ret_addr);
		vm->core.pc = D;
		break;
	}

	case vm_op_Call_Relative: {
		uint16_t ret_addr = vm->core.pc;
		vm_push(vm, ret_addr);
		vm->core.pc += *SR1;
		break;
	}

	case vm_op_Call_Absolute: {
		uint16_t ret_addr = vm->core.pc;
		vm_push(vm, ret_addr);
		vm->core.pc = *R1;
		break;
	}

	case vm_op_Return: vm->core.pc = vm_pop(vm); break;
	}
}
