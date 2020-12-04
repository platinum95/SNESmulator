#include "system.h"

#include "cartridge.h"
#include "cpu.h"
#include "dsp.h"
#include "ppu.h"
#include "ram.h"
#include "spc700.h"

#include <stdio.h>
#include <string.h>

static uint8_t openBus;

void cycle();
bool execute;
unsigned int cycle_counter;
static uint8_t WRAM[ 0x20000 ]; // TODO - maybe move to own file

uint32_t consolidateMemoryAddress( MemoryAddress memoryAddress ) {
    return ( ( (uint32_t) memoryAddress.bank ) << 16 ) | ( (uint32_t)memoryAddress.offset );
}

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
        addressBus.bank -= 0x7E;
        uint32_t wramIndex = consolidateMemoryAddress( addressBus );
        if ( writeLine ) {
            WRAM[ wramLine ] = *dataBus;
        }
        else {
            *dataBus = WRAM[ wramLine ];
        }
    }
    else if ( cartLine ) {
        // Cartridge
        cartridgeMemoryAccess( addressBus, dataBus, writeLine );
    }
    else {
        // ?
        // TODO - possibly open bus?
        printf( "TODO - A-Bus open-bus access\n" );
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
        // TODO
        printf( "TODO - WRAM access through registers\n" );
    }
    else {
        // Open-bus
        printf( "TODO - B-Bus open-bus access\n" );
    }
}
