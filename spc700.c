#include "spc700.h"



uint8_t spc_memory[65536];


void spc700_initialise() {
	spc_memory[0x00f4] = 0xAA;
	spc_memory[0x00f5] = 0xBB;
}

uint8_t *get_spc_memory() {
	return spc_memory;
}


uint8_t *access_spc_snes_mapped(uint16_t addr) {
	addr = addr - 0x2140 + 0x00f4;
	if (addr < 0x00f4 || addr > 0x00f7)
		return NULL;
	else
		return &spc_memory[addr];
}