#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "system.h"

#include <stdbool.h>
#include <stdint.h>

int cartridgeLoadRom( const char* filepath );
void deleteRom();

void cartridgeMemoryAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine );


#endif //CARTRIDGE_H