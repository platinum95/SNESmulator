#ifndef CPU_H
#define CPU_H

#include "cartridge.h"
#include <stdint.h>

void cpuInitialise();

void cpuTick();

// DMA needs this - TODO - consider moving stuff so this isn't accessible
void MemoryAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine );

// true for v-blank, false for h-blank
void vBlank( bool level );
void hBlank( bool level );

#endif //CPU_H