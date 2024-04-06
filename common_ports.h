#ifndef COMMON_PORTS_H
#define COMMON_PORTS_H

#include "vm.h"

#include <stdatomic.h>
#include <stdbool.h>

typedef enum common_port {
	common_port_terminal_input    = 150,
	common_port_terminal_output   = 151,

	common_port_shut_down = 255,
} common_port;

uint16_t common_port_read(void *, uint8_t port_number);
void common_port_write(void *, uint8_t port_number, uint16_t data);

typedef struct common_port_state {
	_Atomic bool wrote_to_shut_down;
} common_port_state;

void vm_install_common_ports(vm_state *, common_port_state *);

#endif // COMMON_PORTS_H
