#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "ops.h"

typedef enum vm_fault {
	vm_fault_none                  = 0x0,
	vm_fault_illegal_instruction   = 0x1,
	vm_fault_divide_by_zero        = 0x2,

	vm_fault_explicitly_requested  = 0xf,
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

typedef struct vm_state {
	vm_core *cores;
	uint8_t core_count;

	vm_ports ports;
	uint8_t memory[UINT16_MAX];
} vm_state;

void vm_init(vm_state *, uint8_t core_count, vm_core *cores);
void vm_step(vm_state *, uint8_t core_index);

char const *vm_padded_reg_name(uint8_t);

char const *vm_op_mnemonic(uint8_t code);
vm_operands vm_op_encoding(uint8_t code);
char const *vm_op_name(uint8_t);
char const *vm_fault_name(vm_fault);

// these point to mutable static memory
char const *vm_disasm(uint8_t, uint8_t, uint8_t);
char const *vm_disasm_pc(vm_state const *, uint8_t core_index);

#endif // VM_H
