#include "common_ports.h"
#include <stddef.h>
#include <stdio.h>

uint16_t common_port_read(void *v_state, uint8_t port_number) {
	common_port_state *state = v_state;
	(void)state;
	switch ((common_port)port_number) {
	case common_port_terminal_input:
		return fgetc(stdin);

	case common_port_terminal_output:
	case common_port_shut_down:
		break;
	}

	return 0xffff;
}

void common_port_write(void *v_state, uint8_t port_number, uint16_t data) {
	common_port_state *state = v_state;
	switch ((common_port)port_number) {
	case common_port_terminal_input:
		break;
	case common_port_terminal_output:
		fputc(data & 0x7f, stdout);
		break;
	case common_port_shut_down:
		state->wrote_to_shut_down = true;
		break;
	}
}

void vm_install_common_ports(vm_state *vm, common_port_state *state) {
	state->wrote_to_shut_down = false;
	vm->ports = (vm_ports){
		.context = state,
		.port_read = common_port_read,
		.port_write = common_port_write,
	};
}
