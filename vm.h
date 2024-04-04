#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "ops.h"

typedef enum vm_fault {
	vm_fault_none = 0x00,
	vm_fault_illegal_instruction,
	vm_fault_divide_by_zero,
} vm_fault;

typedef struct vm_core {
	// on faults pc points to the instruction that faulted

	uint16_t pc;
	uint16_t registers[16];
	uint8_t fault;

	// TODO: interrupts, vectors, etc
} vm_core;

typedef struct vm_ports {
	void *context; // passed to every call to read and write, not touched by the vm itself
	uint16_t (*port_read)(void *context, uint8_t port_number);
	void (*port_write)(void *context, uint8_t port_number, uint16_t data);
} vm_ports;

typedef struct vm {
	// TODO: multi core could be fun
	vm_core core;
	vm_ports ports;
	uint8_t memory[UINT16_MAX];
} vm_state;

void vm_init(vm_state *);
void vm_step(vm_state *);

char const *vm_padded_reg_name(uint8_t);

// these point to mutable static memory
char const *op_name(uint8_t);
char const *vm_disasm(uint8_t, uint8_t, uint8_t);
char const *vm_disasm_pc(vm_state const *);

vm_operands op_encoding(uint8_t code);

#endif // VM_H
