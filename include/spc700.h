#ifndef SPC700_H
#define SPC700_H

#include <stdint.h>

void spc700_initialise();
void spc700_execute_next_instruction();
uint8_t *access_spc_snes_mapped( uint16_t addr );

#endif //SPC700_H