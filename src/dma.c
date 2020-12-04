#include "dma.h"

#include <memory.h>
#include <stdio.h>

static struct ChannelSelect {
    uint8_t DMAChannelSelect;
    uint8_t HDMAChannelSelect;
} channelSelect;

typedef struct DMARegisters {
    uint8_t DMAP;  // 0x43_0   - DMA/HDMA Parameters                                   (FFh)
    uint8_t BBAD;  // 0x43_1   - DMA/HDMA I/O-Bus Address (PPU-Bus aka B-Bus)          (FFh)
    uint8_t A1Tl;  // 0x43_2   - HDMA Table Start Address (low)  / DMA Curr Addr (low) (FFh)
    uint8_t A1Th;  // 0x43_3   - HDMA Table Start Address (high) / DMA Curr Addr (high)(FFh)
    uint8_t A1Tb;  // 0x43_4   - HDMA Table Start Address (bank) / DMA Curr Addr (bank)(xxh)
    uint8_t DASl;  // 0x43_5   - Indirect HDMA Address (low)  / DMA Byte-Counter (low) (FFh)
    uint8_t DASh;  // 0x43_6   - Indirect HDMA Address (high) / DMA Byte-Counter (high)(FFh)
    uint8_t DASb;  // 0x43_7   - Indirect HDMA Address (bank)                          (FFh)
    uint8_t A2Al;  // 0x43_8   - HDMA Table Current Address (low)                      (FFh)
    uint8_t A2Ah;  // 0x43_9   - HDMA Table Current Address (high)                     (FFh)
    uint8_t NTRL;  // 0x43_A   - HDMA Line-Counter (from current Table entry)          (FFh)
    uint8_t UNUSEDb;// 0x43_B   - Unused byte (read/write-able)                         (FFh)
    uint8_t MIRRx;  // 0x43_F   - Mirror of 43xBh (R/W)                                 (FFh)
    // 4380h..5FFFh    - Unused region (open bus)                                -
} DMARegisters;

static DMARegisters dmaRegisters[ 8 ];

void dmaInitialise() {
    channelSelect.DMAChannelSelect = 0x00;
    channelSelect.HDMAChannelSelect = 0x00;

    memset( &dmaRegisters, 0xFF, sizeof( DMARegisters ) * 8 );
}

void dmaTick() {

}

void dmaPortAccess( uint16_t portBus, uint8_t *dataBus, bool writeLine ) {
    
    if ( portBus == 0x420B || portBus == 0x420C ) {
        if ( !writeLine ) {
            // TODO - open-bus
            *dataBus = 0x00;
            return;
        }
        *( ( (uint8_t*) &channelSelect) + portBus - 0x420B ) = *dataBus;
        return;
    }
    else if( portBus < 0x4300 || portBus > 0x437F ) {
        printf( "Invalid DMA register access\n" );
        return;
    }
    else {
        uint8_t channel = (uint8_t)( ( portBus & 0x00F0 ) >> 8 );
        uint8_t offset = (uint8_t)( portBus & 0x000F );
        
        DMARegisters *channelRegisters = &dmaRegisters[ channel ];

        uint8_t *hostAddress = NULL;
        // Switch for better state handling (when needed)
        switch( offset ) {
            case 0x00:
                hostAddress = &channelRegisters->DMAP;
                break;
            case 0x01:
                hostAddress = &channelRegisters->BBAD;
                break;
            case 0x02:
                hostAddress = &channelRegisters->A1Tl;
                break;
            case 0x03:
                hostAddress = &channelRegisters->A1Th;
                break;
            case 0x04:
                hostAddress = &channelRegisters->A1Tb;
                break;
            case 0x05:
                hostAddress = &channelRegisters->DASl;
                break;
            case 0x06:
                hostAddress = &channelRegisters->DASh;
                break;
            case 0x07:
                hostAddress = &channelRegisters->DASb;
                break;
            case 0x08:
                hostAddress = &channelRegisters->A2Al;
                break;
            case 0x09:
                hostAddress = &channelRegisters->A2Ah;
                break;
            case 0x0A:
                hostAddress = &channelRegisters->NTRL;
                break;
            case 0x0B:
                hostAddress = &channelRegisters->UNUSEDb;
                break;
            case 0x0C:
            case 0x0D:
            case 0x0E:
                // TODO - Open-bus
                *dataBus = 0x00;
                return;
            case 0x0F:
                hostAddress = &channelRegisters->UNUSEDb;
                break;
            default:
                // TODO - Error
                *dataBus = 0x00;
                return;
        }

        if ( writeLine ) {
            *hostAddress = *dataBus;
        }
        else {
            *dataBus = *hostAddress;
        }
    }
}