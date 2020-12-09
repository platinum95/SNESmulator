#include "wram.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

static uint8_t WRAM[ 0x20000 ];

typedef struct WRAMPorts {
    uint8_t WMDATA; // 0x2180
    uint8_t WMADDl; // 0x2181
    uint8_t WMADDm; // 0x2182
    uint8_t WMADDh; // 0x2183
} WRAMPorts;

static WRAMPorts ports;

void wramInitialise() {
    // TODO - memset ram?
    memset( &ports, 0x00, sizeof( WRAMPorts ) );
}

void wramBBusAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine ) {
    // Access through port (B-Bus)

    assert( ( addressBus & ~0x83 ) == 0x00 ); // addressBus >= 0x80 && addressBus <= 0x83

    addressBus -= 0x80;
    if ( addressBus == 0x00 ) {
        // WMDATA
        uint32_t wramAddress = ( ( (uint32_t) ( ports.WMADDh & 0x01 ) ) << 16 ) | ( ( (uint32_t) ports.WMADDm ) << 8 ) | ( (uint32_t) ports.WMADDl );
        if( writeLine ) {
            WRAM[ wramAddress ] = *dataBus;
        }
        else {
            *dataBus = WRAM[ wramAddress ];
        }
        if ( ++ports.WMADDl == 0 ) {
            if ( ++ports.WMADDm == 0 ) {
                ++ports.WMADDh;
            }
        }

        return;
    }
    else {
        if ( !writeLine ) {
            printf( "Attempting to read from W/O WRAM port\n" );
            return;
        }
        *( ( (uint8_t*)&ports ) + addressBus ) = *dataBus;
    }
}

void wramABusAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine ) {
    // Access through A-Bus
    
    addressBus.bank &= 0x01;
    uint32_t wramIndex = ( ( (uint32_t)addressBus.bank ) << 16 ) | ( (uint32_t)addressBus.offset );

    if ( writeLine ) {
        WRAM[ wramIndex ] = *dataBus;
    }
    else {
        *dataBus = WRAM[ wramIndex ];
    }
}
