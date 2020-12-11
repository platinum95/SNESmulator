#include "dma.h"

#include "cpu.h"
#include "system.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

#define TEMP_ADDR16_CONCAT( offseth, offsetl ) ( ( ( (uint16_t)offseth ) << 8 ) | (uint16_t)offsetl )
#define TEMP_ADDR32_CONCAT( bank, offseth, offsetl ) ( ( (uint32_t)bank ) << 16 ) | ( (uint32_t) TEMP_ADDR16_CONCAT( offseth, offsetl ) )

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

typedef struct DMAChannelState {
    uint8_t currentPos;
} DMAChannelState;

typedef struct HDMAChannelState {
    MemoryAddress currentMemoryAddress; // Irrespective of direct/indirect
    bool repeat;
    uint8_t linesRemaining;
    bool doTransfer;
} HDMAChannelState;

static DMARegisters dmaRegisters[ 8 ];
static DMAChannelState dmaChannelStates[ 8 ];
static HDMAChannelState hdmaChannelStates[ 8 ];

typedef struct DMAState {
    bool HDMAInProgress;
    uint8_t currentHDMAChannel;
} DMAState;

static DMAState dmaState;

void dmaInitialise() {
    channelSelect.DMAChannelSelect = 0x00;
    channelSelect.HDMAChannelSelect = 0x00;

    memset( &dmaRegisters, 0xFF, sizeof( DMARegisters ) * 8 );
}

static inline void GPDMAChannelTick( uint8_t channel ) {
    static const uint8_t transferUnitSizes[ 8 ] = { 1, 2, 2, 4, 4, 4, 2, 4 };

    DMARegisters *channelRegisters = &dmaRegisters[ channel ];
    DMAChannelState *dmaChannelState = &dmaChannelStates[ channel ];

    uint8_t BBusAddress = channelRegisters->BBAD;

    MemoryAddress ABusAddress = {
        .bank = channelRegisters->A1Tb,
        .offset = ( ( (uint16_t)channelRegisters->A1Th ) << 8 ) | ( (uint16_t)channelRegisters->A1Tl )
    };
    
    const uint8_t transferUnitMode = channelRegisters->DMAP & 0x7;
    const uint8_t transferUnitSize = transferUnitSizes[ transferUnitMode ];
    uint16_t bytesLeft = ( ( (uint16_t)channelRegisters->DASh ) << 8 ) | ( (uint16_t)channelRegisters->DASl );
    uint8_t bytesToTransfer = 0;
    if ( bytesLeft <= transferUnitSize ) {
        bytesToTransfer = bytesLeft;
        bytesLeft = 0;
        channelRegisters->DASl = 0;
        channelRegisters->DASh = 0;
    }
    else {
        bytesToTransfer = transferUnitSize;
        bytesLeft -= transferUnitSize;
        
        channelRegisters->DASl = (uint8_t)( bytesLeft & 0x00FF );
        channelRegisters->DASh = (uint8_t)( ( bytesLeft >> 8 ) & 0x00FF );
    }
    
    uint8_t ABusAddressStepMode = ( channelRegisters->DMAP >> 3 ) & 0x03;
    // TODO - optimise this
    for ( uint8_t i = 0; i < bytesToTransfer; ++i ) {
        // Transfer direction (1 is B->A, 0 is A->B)
        // TODO - A-bus access shoudn't be able to map to B-bus
        if ( channelRegisters->DMAP & 0x80 ) {
            uint8_t dataBus;
            B_BusAccess( BBusAddress, &dataBus, false );
            MemoryAccess( ABusAddress, &dataBus, true );
        }
        else {
            uint8_t dataBus;
            MemoryAccess( ABusAddress, &dataBus, false );
            B_BusAccess( BBusAddress, &dataBus, true );
        }
        
        switch ( ABusAddressStepMode ) {
            case 0x00:
                ++ABusAddress.offset;
                break;
            case 0x02:
                --ABusAddress.offset;
                break;
        }

        switch ( transferUnitMode ) {
            case 0x00:
                // NOP
                assert( bytesToTransfer == 1 );
                break;

            case 0x01:
                assert( bytesToTransfer <= 2 );
                ++BBusAddress;
                break;

            case 0x02:
            case 0x06:
                // NOP
                assert( bytesToTransfer <= 2 );
                break;

            case 0x03:
            case 0x07:
                assert( bytesToTransfer <= 4 );
                if ( i == 1 ){ ++BBusAddress; }
                break;

            case 0x04:
                assert( bytesToTransfer <= 4 );
                ++BBusAddress;
                break;

            case 0x05:
                assert( bytesToTransfer <= 4 );
                if ( i == 0 || i == 2 ) { ++BBusAddress; }
                else if ( i == 1 ) { --BBusAddress; }
                break;
            
        }
    }
    
    channelRegisters->A1Th = (uint8_t)( ( ABusAddress.offset >> 8 ) & 0x00FF );
    channelRegisters->A1Tl = (uint8_t)( ABusAddress.offset & 0x00FF );

    if ( bytesLeft == 0 ) {
        channelSelect.DMAChannelSelect &= ~( 1 << channel );
    }
}

static inline void HDMAChannelTick( uint8_t channel ) {
    static const uint8_t transferUnitSizes[ 8 ] = { 1, 2, 2, 4, 4, 4, 2, 4 };

    DMARegisters *channelRegisters = &dmaRegisters[ channel ];
    HDMAChannelState *hdmaChannelState = &hdmaChannelStates[ channel ];
    bool indirectMode = ( channelRegisters->DMAP >> 6 ) & 0x01;
    
    if ( hdmaChannelState->doTransfer ) {
        if ( channelSelect.DMAChannelSelect & ( 1 << channel ) ) {
            channelSelect.DMAChannelSelect &= ~( 1 << channel );
        }
        uint8_t BBusAddress = channelRegisters->BBAD;

        MemoryAddress ABusAddress;
        if ( indirectMode ) { 
            ABusAddress = (MemoryAddress) {
                .bank = channelRegisters->A1Tb,
                .offset = ( ( (uint16_t)channelRegisters->DASh ) << 8 ) | ( (uint16_t)channelRegisters->DASl )
            };
        }
        else {
            ABusAddress = (MemoryAddress) {
                .bank = channelRegisters->A1Tb,
                .offset = ( ( (uint16_t)channelRegisters->A2Ah ) << 8 ) | ( (uint16_t)channelRegisters->A2Al )
            };
        }

        const uint8_t transferUnitMode = channelRegisters->DMAP & 0x7;
        const uint8_t transferUnitSize = transferUnitSizes[ transferUnitMode ];
        for ( uint8_t i = 0; i < transferUnitSize; ++i ) {
            // Transfer direction (1 is B->A, 0 is A->B)
            // TODO - A-bus access shoudn't be able to map to B-bus
            if ( channelRegisters->DMAP & 0x80 ) {
                uint8_t dataBus;
                B_BusAccess( BBusAddress, &dataBus, false );
                MemoryAccess( ABusAddress, &dataBus, true );
            }
            else {
                uint8_t dataBus;
                MemoryAccess( ABusAddress, &dataBus, false );
                B_BusAccess( BBusAddress, &dataBus, true );
            }
            ++ABusAddress.offset;

            switch ( transferUnitMode ) {
                case 0x00:
                    // NOP
                    break;

                case 0x01:
                    ++BBusAddress;
                    break;

                case 0x02:
                case 0x06:
                    // NOP
                    break;

                case 0x03:
                case 0x07:
                    if ( i == 1 ){ ++BBusAddress; }
                    break;

                case 0x04:
                    ++BBusAddress;
                    break;

                case 0x05:
                    if ( i == 0 || i == 2 ) { ++BBusAddress; }
                    else if ( i == 1 ) { --BBusAddress; }
                    break;
                
            }
        }

        if ( indirectMode ) { 
            channelRegisters->DASh = (uint8_t)( ( ABusAddress.offset >> 8 ) & 0x00FF );
            channelRegisters->DASl = (uint8_t)( ABusAddress.offset & 0x00FF );
        }
        else {
            channelRegisters->A2Ah = (uint8_t)( ( ABusAddress.offset >> 8 ) & 0x00FF );
            channelRegisters->A2Al = (uint8_t)( ABusAddress.offset & 0x00FF );
        }
    }
    --channelRegisters->NTRL;
    if ( ( channelRegisters->NTRL & ~0x80 ) == 0 ) {
        uint16_t offset = TEMP_ADDR16_CONCAT( channelRegisters->A2Ah, channelRegisters->A2Al );
        MemoryAddress entryAddress = {
            .bank = channelRegisters->A1Tb,
            .offset = offset
        };

        MemoryAccess( entryAddress, &channelRegisters->NTRL, false );
        ++entryAddress.offset;

        if ( indirectMode ) {
            MemoryAccess( entryAddress, &channelRegisters->DASl, false );
            ++entryAddress.offset;
            MemoryAccess( entryAddress, &channelRegisters->DASh, false );
            ++entryAddress.offset;
        }
        channelRegisters->A2Ah = (uint8_t)( ( entryAddress.offset >> 8 ) & 0x00FF );
        channelRegisters->A2Al = (uint8_t)( entryAddress.offset & 0x00FF );

        hdmaChannelState->doTransfer = ( channelRegisters->NTRL > 0x00 );
    }
}

static inline void GPDMATick() {
    for ( uint8_t i = 0; i < 8; ++i ) {
        if ( channelSelect.DMAChannelSelect & ( 1 << i ) ) {
            GPDMAChannelTick( i );
            return;
        }
    }
}

static inline void HDMATick() {
    HDMAChannelTick( dmaState.currentHDMAChannel );
    if ( dmaState.currentHDMAChannel == 7 ) {
        dmaState.HDMAInProgress = false;
        dmaState.currentHDMAChannel = 0;
    }
}

bool dmaTick() {
    if ( dmaState.HDMAInProgress ) {
        HDMATick();
        return true;
    }
    else if ( channelSelect.DMAChannelSelect > 0 ) {
        GPDMATick();
        return true;
    }
    return false;
}

void dmaHBlank() {
    if ( channelSelect.HDMAChannelSelect > 0 ) {
        dmaState.HDMAInProgress = true;
        dmaState.currentHDMAChannel = 0;
    }
}

void dmaVBlank( bool level ) {
    if ( level == false ) {
        // Falling-edge, VBlank ending. Reload HDMA Registers
        for ( uint8_t i = 0; i < 8; ++i ) {
            DMARegisters *registers = &dmaRegisters[ i ];
            HDMAChannelState *state = &hdmaChannelStates[ i ];
            if ( ( channelSelect.HDMAChannelSelect & ( 1 << i ) ) == 0 ) {
                state->doTransfer = false;
                continue;
            } 
            state->doTransfer = true;
            
            registers->A2Al = registers->A1Tl;
            registers->A2Ah = registers->A1Th;
            bool indirectMode = ( registers->DMAP >> 6 ) & 0x01;
            MemoryAddress address = {
                .bank = registers->A1Tb,
                .offset = TEMP_ADDR16_CONCAT( registers->A2Ah, registers->A2Al )
            };

            if ( indirectMode ) {
                // Indirect mode, read table address from address
                uint16_t offset;
                uint8_t temp;
                MemoryAccess( address, &temp, false );
                offset = temp;
                ++address.offset;
                MemoryAccess( address, &temp, false );
                offset |= ( (uint16_t)temp ) << 8;
                address.offset = offset;
            }
            uint8_t nrtl;
            MemoryAccess( address, &nrtl, false );
            ++address.offset;
            registers->NTRL = nrtl;
            --nrtl;
            hdmaChannelStates[ i ].currentMemoryAddress = address;
            hdmaChannelStates[ i ].repeat = registers->NTRL & 0x80;
            hdmaChannelStates[ i ].linesRemaining = registers->NTRL & ~0x80;
        }
    }
}

void dmaPortAccess( uint16_t portBus, uint8_t *dataBus, bool writeLine ) {
    
    if ( portBus == 0x420B || portBus == 0x420C ) {
        if ( !writeLine ) {
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
                // Open-bus
                return;
            case 0x0F:
                hostAddress = &channelRegisters->UNUSEDb;
                break;
            default:
                // Invalid address
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