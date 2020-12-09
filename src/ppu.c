#include "ppu.h"

#include "cpu.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

// Just PAL for now, NTSC later

typedef struct Ports {
    // Write-only region
    uint8_t INIDISP; // 2100h - Display Control 1                                  8xh
    uint8_t OBSEL; // 2101h   - Object Size and Object Base                        (?)
    uint8_t OAMADDL; // 2102h - OAM Address (lower 8bit)                           (?)
    uint8_t OAMADDH; // 2103h - OAM Address (upper 1bit) and Priority Rotation     (?)
    uint8_t OAMDATA; // 2104h - OAM Data Write (write-twice)                       (?)
    uint8_t BGMODE; // 2105h  - BG Mode and BG Character Size                      (xFh)
    uint8_t MOSAIC; // 2106h  - Mosaic Size and Mosaic Enable                      (?)
    uint8_t BG1SC; // 2107h   - BG1 Screen Base and Screen Size                    (?)
    uint8_t BG2SC; // 2108h   - BG2 Screen Base and Screen Size                    (?)
    uint8_t BG3SC; // 2109h   - BG3 Screen Base and Screen Size                    (?)
    uint8_t BG4SC; // 210Ah   - BG4 Screen Base and Screen Size                    (?)
    uint8_t BG12NBA; // 210Bh - BG Character Data Area Designation                 (?)
    uint8_t BG34NBA; // 210Ch - BG Character Data Area Designation                 (?)
    uint8_t BG1HOFS; // 210Dh - BG1 Horizontal Scroll (X) (write-twice) / M7HOFS   (?,?)
    uint8_t BG1VOFS; // 210Eh - BG1 Vertical Scroll (Y)   (write-twice) / M7VOFS   (?,?)
    uint8_t BG2HOFS; // 210Fh - BG2 Horizontal Scroll (X) (write-twice)            (?,?)
    uint8_t BG2VOFS; // 2110h - BG2 Vertical Scroll (Y)   (write-twice)            (?,?)
    uint8_t BG3HOFS; // 2111h - BG3 Horizontal Scroll (X) (write-twice)            (?,?)
    uint8_t BG3VOFS; // 2112h - BG3 Vertical Scroll (Y)   (write-twice)            (?,?)
    uint8_t BG4HOFS; // 2113h - BG4 Horizontal Scroll (X) (write-twice)            (?,?)
    uint8_t BG4VOFS; // 2114h - BG4 Vertical Scroll (Y)   (write-twice)            (?,?)
    uint8_t VMAIN; // 2115h   - VRAM Address Increment Mode                        (?Fh)
    uint8_t VMADDL; // 2116h  - VRAM Address (lower 8bit)                          (?)
    uint8_t VMADDH; // 2117h  - VRAM Address (upper 8bit)                          (?)
    uint8_t VMDATAL; // 2118h - VRAM Data Write (lower 8bit)                       (?)
    uint8_t VMDATAH; // 2119h - VRAM Data Write (upper 8bit)                       (?)
    uint8_t M7SEL; // 211Ah   - Rotation/Scaling Mode Settings                     (?)
    uint8_t M7A; // 211Bh     - Rotation/Scaling Parameter A & Maths 16bit operand(FFh)(w2)
    uint8_t M7B; // 211Ch     - Rotation/Scaling Parameter B & Maths 8bit operand (FFh)(w2)
    uint8_t M7C; // 211Dh     - Rotation/Scaling Parameter C         (write-twice) (?)
    uint8_t M7D; // 211Eh     - Rotation/Scaling Parameter D         (write-twice) (?)
    uint8_t M7X; // 211Fh     - Rotation/Scaling Center Coordinate X (write-twice) (?)
    uint8_t M7Y; // 2120h     - Rotation/Scaling Center Coordinate Y (write-twice) (?)
    uint8_t CGADD; // 2121h   - Palette CGRAM Address                              (?)
    uint8_t CGDATA; // 2122h  - Palette CGRAM Data Write             (write-twice) (?)
    uint8_t W12SEL; // 2123h  - Window BG1/BG2 Mask Settings                       (?)
    uint8_t W34SEL; // 2124h  - Window BG3/BG4 Mask Settings                       (?)
    uint8_t WOBJSEL; // 2125h - Window OBJ/MATH Mask Settings                      (?)
    uint8_t WH0; // 2126h     - Window 1 Left Position (X1)                        (?)
    uint8_t WH1; // 2127h     - Window 1 Right Position (X2)                       (?)
    uint8_t WH2; // 2128h     - Window 2 Left Position (X1)                        (?)
    uint8_t WH3; // 2129h     - Window 2 Right Position (X2)                       (?)
    uint8_t WBGLOG; // 212Ah  - Window 1/2 Mask Logic (BG1-BG4)                    (?)
    uint8_t WOBJLOG; // 212Bh - Window 1/2 Mask Logic (OBJ/MATH)                   (?)
    uint8_t TM; // 212Ch      - Main Screen Designation                            (?)
    uint8_t TS; // 212Dh      - Sub Screen Designation                             (?)
    uint8_t TMW; // 212Eh     - Window Area Main Screen Disable                    (?)
    uint8_t TSW; // 212Fh     - Window Area Sub Screen Disable                     (?)
    uint8_t CGWSEL; // 2130h  - Color Math Control Register A                      (?)
    uint8_t CGADSUB; // 2131h - Color Math Control Register B                      (?)
    uint8_t COLDATA; // 2132h - Color Math Sub Screen Backdrop Color               (?)
    uint8_t SETINI; // 2133h  - Display Control 2                                  00h?

    // Read-only region
    uint8_t MPYL; // 2134h    - PPU1 Signed Multiply Result   (lower 8bit)         (01h)
    uint8_t MPYM; // 2135h    - PPU1 Signed Multiply Result   (middle 8bit)        (00h)
    uint8_t MPYH; // 2136h    - PPU1 Signed Multiply Result   (upper 8bit)         (00h)
    uint8_t SLHV; // 2137h    - PPU1 Latch H/V-Counter by Software (Read=Strobe)
    uint8_t RDOAM; // 2138h   - PPU1 OAM Data Read            (read-twice)
    uint8_t RDVRAML; // 2139h - PPU1 VRAM Data Read           (lower 8bits)
    uint8_t RDVRAMH; // 213Ah - PPU1 VRAM Data Read           (upper 8bits)
    uint8_t RDCGRAM; // 213Bh - PPU2 CGRAM Data Read (Palette)(read-twice)
    uint8_t OPHCT; // 213Ch   - PPU2 Horizontal Counter Latch (read-twice)         (01FFh)
    uint8_t OPVCT; // 213Dh   - PPU2 Vertical Counter Latch   (read-twice)         (01FFh)
    uint8_t STAT77; // 213Eh  - PPU1 Status and PPU1 Version Number
    uint8_t STAT78; // 213Fh  - PPU2 Status and PPU2 Version Number                Bit7=0
} Ports;

static uint8_t VRAM[ 0x8000 ]; // 32KB
static uint8_t CGRAM[ 0x200 ]; // 512B
static uint8_t OAMRAM[ 0x200 + 0x20 ]; // 512B + 32B

#define H_BLANK_BOUNDARY 256
#define H_MAX 340
#define V_BLANK_BOUNDARY 240
#define V_MAX 312

typedef struct PPUState {
    uint16_t vCount;
    uint16_t hCount;
    uint8_t countDiv;
    bool hBlank;
    bool vBlank;
    bool fBlank;

    bool cgramSecondAccess;
    uint8_t CGRAM_lsb_latch;

    bool oamramSecondAccess;
    uint8_t oamramLsbLatch;
    uint16_t oamramAddress;
} PPUState;

static Ports ports;
static PPUState ppuState;

void ppuInitialise() {
    memset( &ports, 0x00, sizeof( Ports ) );
    memset( &ppuState, 0x00, sizeof( PPUState ) );
}

static inline void vInc() {
    ++ppuState.vCount;
    if ( ppuState.vCount == V_BLANK_BOUNDARY ) {
        vBlank( true );
    }
    else if ( ppuState.vCount == V_MAX ) {
        ppuState.vCount = 0;
        vBlank( false );
    }
}

static inline void hInc() {
    ++ppuState.hCount;
    if ( ppuState.hCount == H_BLANK_BOUNDARY ) {
        hBlank( true );
    }
    else if ( ppuState.hCount == H_MAX ) {
        ppuState.hCount = 0;
        hBlank( false );
        vInc();
    }
}

void ppuTick() {
    // TODO
    if ( ++ppuState.countDiv == 4 ) {
        ppuState.countDiv = 0;
        hInc();
    }

}

static inline void prefetchRead() {
    uint16_t offset = ( ( (uint16_t)ports.VMADDH ) << 8 ) | ( (uint16_t)ports.VMADDL );
    ports.RDVRAML = VRAM[ offset ];
    ports.RDVRAMH = VRAM[ offset + 1 ];
}

static inline void incrementVMADDR( bool highByte ) {
    static const uint16_t incSteps[ 4 ] = { 2, 64, 256, 256 };
    uint8_t incMode = ports.VMAIN;
    uint16_t incStep = incSteps[ incMode & 0x03 ];
    bool incHighByte = ( incMode & 0x80 );
    if ( incHighByte ==  highByte ) {
        uint16_t addr = ( (uint16_t)( ports.VMADDH << 8 ) ) | ( (uint16_t)ports.VMADDL );
        addr += incStep;
        ports.VMADDL = (uint8_t)( addr & 0x00FF );
        ports.VMADDH = (uint8_t)( ( addr >> 8 ) & 0x00FF );
    }

    if ( incMode & 0x0C ) {
        // TODO - rotate
        printf( "TODO - VMADDR translation\n" );
    }
}

void ppuPortAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine ) {
    
    assert( addressBus <= 0x3F );

    // TODO - explicit handling for state machine
    if ( addressBus <= 0x33 ) {
        if ( !writeLine ) {
            // Write-only
            printf( "Attempting to read W/O PPU port\n" );
            return;
        }
        else {
            switch( addressBus ) {
                case 0x02: {
                    // OAMADDL
                    ports.OAMADDL = *dataBus;
                    ppuState.oamramAddress &= ~0x1FF;
                    ppuState.oamramAddress |= ( (uint16_t) *dataBus ) << 1;
                    break;
                }
                case 0x03: {
                    // OAMADDL
                    ports.OAMADDH = *dataBus;
                    ppuState.oamramAddress &= ~0x200;
                    ppuState.oamramAddress |= ( (uint16_t) ( *dataBus & 0x01 ) ) << 9;
                    break;
                }
                case 0x04: {
                    // OAMDATA
                    OAMRAM[ ppuState.oamramAddress ] = *dataBus;
                    ++ppuState.oamramAddress;
                    ppuState.oamramAddress &= 0x1FF;
                    break;
                }
                case 0x16:
                case 0x17:
                    // VMADDl/h
                    prefetchRead();
                    break;
                case 0x18:
                case 0x19: {
                    uint8_t offset = addressBus - 0x18;
                    // TODO - VRAM accesses
                    uint16_t addr = ( (uint16_t)( ports.VMADDH << 8 ) ) | ( (uint16_t)ports.VMADDL );
                    addr += offset;
                    addr &= ~0x8000;
                    VRAM[ addr ] =  *dataBus;
                    prefetchRead(); // TODO - prefetch before or after?
                    incrementVMADDR( (bool)offset );
                    break;
                }
                case 0x21:
                    ppuState.cgramSecondAccess = false;
                    ports.CGADD = *dataBus;
                    break;
                case 0x22: {
                    // CGDATA
                    if ( !ppuState.cgramSecondAccess ) {
                        // Even address
                        ppuState.CGRAM_lsb_latch = *dataBus;
                        ppuState.cgramSecondAccess = true;
                    }
                    else {
                        // TODO - open-bus upper-bit
                        uint16_t addr = ports.CGADD * 2;
                        CGRAM[ addr ] = *dataBus;
                        CGRAM[ addr - 1 ] = ppuState.CGRAM_lsb_latch;
                        ppuState.cgramSecondAccess = false;
                        ++ports.CGADD;
                    }
                    break;
                }
                default: {
                    uint8_t *port = ( (uint8_t*)&ports ) + ( addressBus );
                    *port = *dataBus;
                    break;
                }
            }
        }
    }
    else {
        if ( writeLine ) {
            // Read-only
            printf( "Attempting to write R/O PPU port\n" );
            return;
        }
        else {
            uint8_t *port = ( (uint8_t*)&ports ) + ( addressBus );
            *dataBus = *port;
            switch( addressBus ) {
                case 0x38: {
                    // RDOAM
                    *dataBus = OAMRAM[ ppuState.oamramAddress ];
                    ++ppuState.oamramAddress;
                    ppuState.oamramAddress &= 0x1FF;
                    break;
                }
                case 0x39:
                case 0x3A: {
                    prefetchRead();
                    uint8_t offset = addressBus - 0x39;
                    incrementVMADDR( (bool)offset );
                    break;
                }
                case 0x3B: {
                    // RDCGRAM
                    // TODO - open-bus upper-bit on odd address
                    uint16_t addr = ports.CGADD * 2;
                    if ( !ppuState.cgramSecondAccess ) {
                        *dataBus = CGRAM[ addr ];
                        ppuState.cgramSecondAccess = true;
                    }
                    else {
                        *dataBus = CGRAM[ addr + 1 ];
                        ppuState.cgramSecondAccess = false;
                        ++ports.CGADD;
                    }
                }
            }
        }
    }
}
