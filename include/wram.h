#ifndef WRAM_H
#define WRAM_h

#include "system.h"

#include <stdbool.h>
#include <stdint.h>

void wramInitialise();
void wramBBusAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine );
void wramABusAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine );

#endif // WRAM_H