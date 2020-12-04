#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

void dmaInitialise();
void dmaTick();
void dmaPortAccess( uint16_t portBus, uint8_t *dataBus, bool writeLine );

#endif //DMA_H