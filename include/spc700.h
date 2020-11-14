#ifndef SPC700_H
#define SPC700_H

#include <stdint.h>

void spc700_initialise();

uint8_t *get_spc_memory();

uint8_t *access_spc_snes_mapped(uint16_t addr);

#endif //SPC700_H