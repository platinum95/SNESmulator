#ifndef CPU_H
#define CPU_H

#include "cartridge.h"
#include <stdint.h>

void InitialiseCpu( RomTypes romType );

void ExecuteNextInstruction();

#endif //CPU_H