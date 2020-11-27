#ifndef SPC700_H
#define SPC700_H

#include <stdbool.h>
#include <stdint.h>

void spc700_initialise();
void spc700_execute_next_instruction();
uint8_t *accessSpcComPort( uint8_t port, bool writeLine );

#endif //SPC700_H