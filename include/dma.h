#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

void dmaInitialise();
// Returns true if DMA in progress
bool dmaTick();
void dmaPortAccess( uint16_t portBus, uint8_t *dataBus, bool writeLine );
void dmaHBlank();
void dmaVBlank( bool level );

#endif //DMA_H