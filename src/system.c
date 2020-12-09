#include "system.h"

#include "cartridge.h"
#include "cpu.h"
#include "dsp.h"
#include "ppu.h"
#include "wram.h"
#include "spc700.h"

#include <stdio.h>
#include <string.h>

static uint8_t openBus;

void cycle();
bool execute;
unsigned int cycle_counter;

int startup() {
    cpuInitialise();
    spc700Initialise();
    ppuInitialise();
    dspInitialise();

    return 0;
}

void begin_execution() {
    execute = 1;
    cycle();
}

void cycle() {
    while ( execute ) {
        cpuTick();
        spc700Tick();
        dspTick();
        ppuTick();
        cycle_counter++;
    }
}

void A_BusAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine, bool wramLine, bool cartLine ) {
    // TODO - maybe validate the parameters
    // TODO - might be able to split this into separate functions
    if ( wramLine ) {
        // Work ram
        wramABusAccess( addressBus, dataBus, writeLine );
    }
    else if ( cartLine ) {
        // Cartridge
        cartridgeMemoryAccess( addressBus, dataBus, writeLine );
    }
    else {
        // Possibly open bus?
    }
}

void B_BusAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine ) {
    // APU, PPU
    // 0x00 -> 0xFF { 0x2100 -> 0x21FF }
    if ( addressBus <= 0x3F ) {
        // PPU
        ppuPortAccess( addressBus, dataBus, writeLine );
        return;
    }
    else if ( addressBus <= 0x7F ) {
        // APU
        spc700PortAccess( addressBus, dataBus, writeLine );
    }
    else if ( addressBus <= 0x83 ) {
        // WRAM access
        wramBBusAccess( addressBus, dataBus, writeLine );
    }
    else {
        // Open-bus
    }
}
