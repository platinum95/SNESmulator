#ifndef CPU_INTERNAL_H
#define CPU_INTERNAL_H
#include "system.h"

void MemoryAccess( MemoryAddress addressBus, uint8_t *data, bool writeLine );

uint8_t MainBusReadU8( MemoryAddress addressBus );
uint16_t MainBusReadU16( MemoryAddress addressBus );
uint32_t MainBusReadU24( MemoryAddress addressBus );

void MainBusWriteU8( MemoryAddress addressBus, uint8_t value );
void MainBusWriteU16( MemoryAddress addressBus, uint16_t value );
void MainBusWriteU32( MemoryAddress addressBus, uint32_t value );

#endif// CPU_INTERNAL_H