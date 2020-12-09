#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

void ppuInitialise();
void ppuTick();
void ppuPortAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine );
void ppuInterruptStateAccess( uint8_t offset, uint8_t *dataBus, bool writeLine );

#endif //PPU_H