#ifndef COMMON_PORTS_H
#define COMMON_PORTS_H

#include "vm.h"

typedef enum common_port {
	common_port_terminal_input    = 150,
	common_port_terminal_output   = 151,
} common_port;

uint16_t port_read(void *, uint8_t port_number);
void port_write(void *, uint8_t port_number, uint16_t data);

void vm_install_common_ports(vm_state *);

#endif // COMMON_PORTS_H
