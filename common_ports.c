#include "common_ports.h"
#include <stddef.h>
#include <stdio.h>

uint16_t port_read(void *_, uint8_t port_number) {
	(void)_;
	switch ((common_port)port_number) {
	case common_port_terminal_input:
		return getchar();
	case common_port_terminal_output:
		break;
	}

	return 0xffff;
}

void port_write(void *_, uint8_t port_number, uint16_t data) {
	(void)_;
	switch ((common_port)port_number) {
	case common_port_terminal_input:
		break;
	case common_port_terminal_output:
		putchar((uint8_t)data);
		break;
	}
}

void vm_install_common_ports(vm_state *vm) {
	vm->ports = (vm_ports){
		.context = NULL,
		.port_read = port_read,
		.port_write = port_write,
	};
}
