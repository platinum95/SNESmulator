#ifndef SPC700_H
#define SPC700_H

#include <stdbool.h>
#include <stdint.h>

void spc700_initialise();
void spc700_execute_next_instruction();
void accessSpcComPort( uint8_t port, uint8_t *dataBus, bool writeLine );

uint8_t spcMemoryMapRead( uint16_t addr );
// TODO - readU16 may need to know if page can increment, or just offset
uint16_t spcMemoryMapReadU16( uint16_t addr );
void spcMemoryMapWrite( uint16_t addr, uint8_t value );
// TODO - writeU16 may need to know if page can increment, or just offset
void spcMemoryMapWriteU16( uint16_t addr, uint16_t value );
#endif //SPC700_H