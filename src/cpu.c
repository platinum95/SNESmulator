/*
*
*/

#include "cpu.h"
#include "cpu_internal.h"

#include "core_65816.h"
#include "dma.h"
#include "system.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* TODO:
    - Should get/set ACC,X,Y with through the getters/setters for proper endian handling
    - Check X_FLAG usage
    - P reg to 8 bits
*/
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))

static uint8_t MDR;

void cpuInitialise() {
    coreInitialise();
}

void IRQ( bool level ) {
    coreIRQ( level );
}

static uint8_t RDNMI;
static uint8_t NMITIMEN;

// TODO - move
typedef struct TimerState {
    bool vBlankLevel;
    bool hBlankLevel;
    uint8_t HVBJOY;
    uint16_t vCount;
    uint16_t hCount;
    uint16_t VTIME;
    uint16_t HTIME;
    uint8_t TIMEUP;
    uint16_t countDivisor;
    bool vBlankIRQEnable; 
    bool hBlankIRQEnable;
} TimerState;

static TimerState timerState;

void vBlank( bool level ) {
    if ( level == false ) {
        timerState.vCount = 0;
        timerState.HVBJOY &= ~80;
    }
    else {
        if ( ( ( RDNMI & 0x80 ) == 0 ) && ( NMITIMEN & 0x80 ) ) {
            // TODO - InternalNMIFlag = true;
            coreNMI( true );
        }
        RDNMI |= 0x80;
        timerState.HVBJOY |= 0x80;
    }
    timerState.vBlankLevel = level;
    dmaVBlank( level );
}

void hBlank( bool level ) {
    timerState.HVBJOY &= ~40;
    if ( timerState.hBlankLevel == true ) {
        timerState.hCount = 0;
        dmaHBlank();
    }
    else {
        timerState.HVBJOY |= 40;
    }
    timerState.hBlankLevel = level;
}

static inline void triggerBlankIRQ() {
    timerState.TIMEUP = 0x80;
    IRQ( false );
}

static inline void vTimerTick() {
    // TODO - verify timing
    ++timerState.vCount;
    if ( timerState.vCount == timerState.VTIME && timerState.vBlankIRQEnable ) {
        // IRQ trigger
        triggerBlankIRQ();
    }
    // TODO - get actual boundary
    if ( timerState.vCount == 264 ) {
        // IRQ trigger
        timerState.vCount = 0;
        IRQ( true );
    }
}

static inline void hTimerTick() {
    // TODO - verify timing
    if ( ++timerState.countDivisor % 4 == 0 ) {
        ++timerState.hCount;
        if ( timerState.hCount == timerState.HTIME && timerState.hBlankIRQEnable ) {
            // IRQ trigger
            triggerBlankIRQ();
        }
        // TODO - get actual boundary
        if ( timerState.hCount == 264 ) {
            vTimerTick();
            timerState.hCount = 0;
            IRQ( true );
        }
    }
}

void cpuTick() {
    hTimerTick();
    if ( !dmaTick() ) {
        coreTick();
    }
}

/*
//CPU On-Chip I/O Ports (Write-only) (Read=open bus)
typedef struct CPUIORegistersB {
4200h - NMITIMEN- Interrupt Enable and Joypad Request                   00h
4201h - WRIO    - Joypad Programmable I/O Port (Open-Collector Output)  FFh
4202h - WRMPYA  - Set unsigned 8bit Multiplicand                        (FFh)
4203h - WRMPYB  - Set unsigned 8bit Multiplier and Start Multiplication (FFh)
4204h - WRDIVL  - Set unsigned 16bit Dividend (lower 8bit)              (FFh)
4205h - WRDIVH  - Set unsigned 16bit Dividend (upper 8bit)              (FFh)
4206h - WRDIVB  - Set unsigned 8bit Divisor and Start Division          (FFh)
4207h - HTIMEL  - H-Count Timer Setting (lower 8bits)                   (FFh)
4208h - HTIMEH  - H-Count Timer Setting (upper 1bit)                    (01h)
4209h - VTIMEL  - V-Count Timer Setting (lower 8bits)                   (FFh)
420Ah - VTIMEH  - V-Count Timer Setting (upper 1bit)                    (01h)
420Bh - MDMAEN  - Select General Purpose DMA Channel(s) and Start Transfer 0
420Ch - HDMAEN  - Select H-Blank DMA (H-DMA) Channel(s)                    0
420Dh - MEMSEL  - Memory-2 Waitstate Control                               0
//420Eh..420Fh    - Unused region (open bus)                                 -
} CPUIORegistersB;
*/

/*
//CPU On-Chip I/O Ports (Read-only)
typedef struct CPUIORegistersC {
4210h - RDNMI   - V-Blank NMI Flag and CPU Version Number (Read/Ack)      0xh
4211h - TIMEUP  - H/V-Timer IRQ Flag (Read/Ack)                           00h
4212h - HVBJOY  - H/V-Blank flag and Joypad Busy flag (R)                 (?)
4213h - RDIO    - Joypad Programmable I/O Port (Input)                    -
4214h - RDDIVL  - Unsigned Division Result (Quotient) (lower 8bit)        (0)
4215h - RDDIVH  - Unsigned Division Result (Quotient) (upper 8bit)        (0)
4216h - RDMPYL  - Unsigned Division Remainder / Multiply Product (lower 8bit)
4217h - RDMPYH  - Unsigned Division Remainder / Multiply Product (upper 8bit)
4218h - JOY1L   - Joypad 1 (gameport 1, pin 4) (lower 8bit)               00h
4219h - JOY1H   - Joypad 1 (gameport 1, pin 4) (upper 8bit)               00h
421Ah - JOY2L   - Joypad 2 (gameport 2, pin 4) (lower 8bit)               00h
421Bh - JOY2H   - Joypad 2 (gameport 2, pin 4) (upper 8bit)               00h
421Ch - JOY3L   - Joypad 3 (gameport 1, pin 5) (lower 8bit)               00h
421Dh - JOY3H   - Joypad 3 (gameport 1, pin 5) (upper 8bit)               00h
421Eh - JOY4L   - Joypad 4 (gameport 2, pin 5) (lower 8bit)               00h
421Fh - JOY4H   - Joypad 4 (gameport 2, pin 5) (upper 8bit)               00h
//4220h..42FFh    - Unused region (open bus)                                -
} CPUIORegistersC;
*/

static inline void CPUWriteOnlyIOPortAccess( uint16_t registerBus, uint8_t *dataBus ) {
    // TODO
    uint16_t offset = registerBus - 0x4200;
    switch ( offset ) {
        case 0x0000:
            // NMITIMEN- Interrupt Enable and Joypad Request 00h
            NMITIMEN = *dataBus;
            timerState.hBlankIRQEnable = NMITIMEN & ( 1u << 4 );
            timerState.vBlankIRQEnable = NMITIMEN & ( 1u << 5 );

            if ( ( ( NMITIMEN & 0x80 ) == 0 ) && ( RDNMI & 0x80 ) ) {
                // TODO - InternalNMIFlag = true;
                coreNMI( true );
            }
            break;
        case 0x0001:
            // TODO - WRIO    - Joypad Programmable I/O Port (Open-Collector Output)  FFh
            printf( "TODO - WRIO\n" );
            break;
        case 0x0002:
            // TODO - WRMPYA  - Set unsigned 8bit Multiplicand (FFh)
            printf( "TODO - WRMPYA\n" );
            break;
        case 0x0003:
            // TODO - WRMPYB  - Set unsigned 8bit Multiplier and Start Multiplication (FFh)
            printf( "TODO - WRMPYB\n" );
            break;
        case 0x0004:
            // TODO - WRDIVL  - Set unsigned 16bit Dividend (lower 8bit) (FFh)
            printf( "TODO - WRDIVL\n" );
            break;
        case 0x0005:
            // TODO - WRDIVH  - Set unsigned 16bit Dividend (upper 8bit) (FFh)
            printf( "TODO - WRDIVH\n" );
            break;
        case 0x0006:
            // TODO - WRDIVB  - Set unsigned 8bit Divisor and Start Division (FFh)
            printf( "TODO - WRDIVB\n" );
            break;
        case 0x0007:
            // HTIMEL  - H-Count Timer Setting (lower 8bits) (FFh)
            timerState.HTIME = ( timerState.HTIME & 0xFF00 ) | ( (uint16_t) *dataBus );
            break;
        case 0x0008:
            // HTIMEH  - H-Count Timer Setting (upper 1bit) (01h)
            timerState.HTIME = ( ( (uint16_t) ( *dataBus & 0x01 )  ) << 8 ) | ( timerState.HTIME & 0x00FF );
            break;
        case 0x0009:
            // VTIMEL  - V-Count Timer Setting (lower 8bits) (FFh)
            timerState.VTIME = ( timerState.VTIME & 0xFF00 ) | ( (uint16_t) *dataBus );
            break;
        case 0x000A:
            // VTIMEH  - V-Count Timer Setting (upper 1bit) (01h)
            timerState.VTIME = ( ( (uint16_t) ( *dataBus & 0x01 )  ) << 8 ) | ( timerState.VTIME & 0x00FF );
            break;
        case 0x000B:
        case 0x000C:
            // MDMAEN  - Select General Purpose DMA Channel(s) and Start Transfer 0
            // HDMAEN  - Select H-Blank DMA (H-DMA) Channel(s) 0
            dmaPortAccess( registerBus, dataBus, true );
            break;
        case 0x000D:
            // TODO - MEMSEL  - Memory-2 Waitstate Control 0
            printf( "TODO - MEMSEL\n" );
            break;
        default:
            // Open bus
            break;
    }
}

static inline void CPUReadOnlyIOPortAccess( uint16_t registerBus, uint8_t *dataBus ) {
    // TODO
    uint16_t offset = registerBus - 0x4210;
    switch ( offset ) {
        case 0x00:
            //RDNMI   - V-Blank NMI Flag and CPU Version Number (Read/Ack)      0xh
            *dataBus = RDNMI;
            RDNMI = 0x01;
            break;
        case 0x01:
            // TODO - TIMEUP  - H/V-Timer IRQ Flag (Read/Ack)                           00h
            printf( "TODO - TIMEUP\n" );
            break;
        case 0x02:
            // TODO - HVBJOY  - H/V-Blank flag and Joypad Busy flag (R)                 (?)
            printf( "TODO - HVBJOY\n" );
            break;
        case 0x03:
            // TODO - RDIO    - Joypad Programmable I/O Port (Input)                    -
            printf( "TODO - RDIO\n" );
            break;
        case 0x04:
            // TODO - RDDIVL  - Unsigned Division Result (Quotient) (lower 8bit)        (0)
            printf( "TODO - RDDIVL\n" );
            break;
        case 0x05:
            // TODO - RDDIVH  - Unsigned Division Result (Quotient) (upper 8bit)        (0)
            printf( "TODO - RDDIVH\n" );
            break;
        case 0x06:
            // TODO - RDMPYL  - Unsigned Division Remainder / Multiply Product (lower 8bit)
            printf( "TODO - RDMPYL\n" );
            break;
        case 0x07:
            // TODO - RDMPYH  - Unsigned Division Remainder / Multiply Product (upper 8bit)
            printf( "TODO - RDMPYH\n" );
            break;
        case 0x08:
            // TODO - JOY1L   - Joypad 1 (gameport 1, pin 4) (lower 8bit)               00h
            printf( "TODO - JOY1L\n" );
            break;
        case 0x09:
            // TODO - JOY1H   - Joypad 1 (gameport 1, pin 4) (upper 8bit)               00h
            printf( "TODO - JOY1H\n" );
            break;
        case 0x0A:
            // TODO - JOY2L   - Joypad 2 (gameport 2, pin 4) (lower 8bit)               00h
            printf( "TODO - JOY2L\n" );
            break;
        case 0x0B:
            // TODO - JOY2H   - Joypad 2 (gameport 2, pin 4) (upper 8bit)               00h
            printf( "TODO - JOY2H\n" );
            break;
        case 0x0C:
            // TODO - JOY3L   - Joypad 3 (gameport 1, pin 5) (lower 8bit)               00h
            printf( "TODO - JOY3L\n" );
            break;
        case 0x0D:
            // TODO - JOY3H   - Joypad 3 (gameport 1, pin 5) (upper 8bit)               00h
            printf( "TODO - JOY3H\n" );
            break;
        case 0x0E:
            // TODO - JOY4L   - Joypad 4 (gameport 2, pin 5) (lower 8bit)               00h
            printf( "TODO - JOY4L\n" );
            break;
        case 0x0F:
            // TODO - JOY4H   - Joypad 4 (gameport 2, pin 5) (upper 8bit)               00h
            printf( "TODO - JOY4H\n" );
            break;
        default:
            // Open bus
        break;
    }
}

static inline void CPURegisterAccess( uint16_t registerBus, uint8_t *dataBus, bool writeLine ) {
    if ( registerBus <= 0x4015 ) {
        // Open bus
    }
    else if ( registerBus <= 0x4017 ) {
        if ( registerBus == 0x4016 ) {
            if ( writeLine ) {
                // TODO - JOYWR - Joypad Output (W)
                printf( "TODO - JOYWR\n" );
            }
            else {
                // TODO - JOYA - Joypad Input Register A (R)
                printf( "TODO - JOYA (R)\n" );
            }
        }
        else if ( !writeLine ) {
            // TODO - JOYB - Joypad Input Register B (R)
            printf( "TODO - JOYB (R)\n" );
        }
        else {
            // TODO - error, attempting to write to JOYB
            printf( "Attempting to write to JOYB\n" );
        }
    }
    else if ( registerBus <= 0x41FF ) {
        // Open-bus
    }
    else if ( registerBus <= 0x420D && writeLine ) {
        CPUWriteOnlyIOPortAccess( registerBus, dataBus );
    }
    else if ( registerBus <= 0x420F ) {
        // Open-bus
    }
    else if ( registerBus <= 0x421F && !writeLine ) {
        // Read-only IO ports
        CPUReadOnlyIOPortAccess( registerBus, dataBus );
    }
    else if ( registerBus <= 0x42FF ) {
        // Open-bus
    }
    else if ( registerBus <= 0x437F ) {
        dmaPortAccess( registerBus, dataBus, writeLine );
    }
    else if ( registerBus <= 0x5FFF ) {
        // Open-bus
    }
    else {
        // TODO - error
        printf( "Invalid CPU IO register address\n" );
    }   
}

#pragma region CPUMemoryMap
// TODO - move to common file
void MemoryAccess( MemoryAddress addressBus, uint8_t *data, bool writeLine ) {
    if ( writeLine ) {
        MDR = *data;
    }
    uint8_t *dataBus = &MDR;

    if ( addressBus.bank <= 0x3F ) {
        if ( addressBus.offset <= 0x1FFF ) {
            // Address bus A + /WRAM
            // Always lower 8K of WRAM, so adjust the address accordingly
            addressBus.bank = 0x00;
            A_BusAccess( addressBus, dataBus, writeLine, true, false );
        }
        if ( addressBus.offset <= 0x20FF ) {
            // Address bus A
            // Possibly open-bus?
        }
        else if ( addressBus.offset <= 0x21FF ) {
            // Address bus B
            // B-Bus I/O ports
            B_BusAccess( addressBus.offset & 0x00FF, dataBus, writeLine );
        }
        else if ( addressBus.offset <= 0x3FFF ) {
            // Address bus A
            // Possibly open bus?
        }
        else if ( addressBus.offset <= 0x437F ) {
            // Internal CPU registers
            CPURegisterAccess( addressBus.offset, dataBus, writeLine );
        }
        else if ( addressBus.offset <= 0x5FFF ) {
            // Open-bus
        }
        else if ( addressBus.offset <= 0x7FFF ) {
            // TODO - Expansion (A-bus probably)
            // For now, just call onto A-bus with /CART
            A_BusAccess( addressBus, dataBus, writeLine, false, true );
        }
        else {
            // Address bus A + /CART
            A_BusAccess( addressBus, dataBus, writeLine, false, true );
        }
    }
    else if ( addressBus.bank <= 0x7D ) {
        // Address bus A + /CART
        A_BusAccess( addressBus, dataBus, writeLine, false, true );
    }
    else if ( addressBus.bank <= 0x7F ) {
        // Address bus A + /WRAM
        addressBus.bank &= 0x01;
        A_BusAccess( addressBus, dataBus, writeLine, true, false );
    }
    else if ( addressBus.bank <= 0xBF ) {
        if ( addressBus.offset <= 0x1FFF ) {
            // Address bus A + /WRAM
            // Always lower 8K of WRAM, so adjust the address accordingly
            addressBus.bank = 0x00;
            A_BusAccess( addressBus, dataBus, writeLine, true, false );
        }
        if ( addressBus.offset <= 0x20FF ) {
            // Address bus A
            // Possibly open-bus?
        }
        else if ( addressBus.offset <= 0x21FF ) {
            // Address bus B
            // B-Bus I/O ports
            B_BusAccess( addressBus.offset & 0x00FF, dataBus, writeLine );
        }
        else if ( addressBus.offset <= 0x3FFF ) {
            // Address bus A
            // Possbile open bus?
        }
        else if ( addressBus.offset <= 0x437F ) {
            // Internal CPU registers
            CPURegisterAccess( addressBus.offset, dataBus, writeLine );
        }
        else if ( addressBus.offset <= 0x5FFF ) {
            // Open-bus
        }
        else if ( addressBus.offset <= 0x7FFF ) {
            // TODO - Expansion (A-bus probably)
            // For now, just call onto A-bus with /CART
            A_BusAccess( addressBus, dataBus, writeLine, false, true );
        }
        else {
            // Address bus A + /CART
            A_BusAccess( addressBus, dataBus, writeLine, false, true );
        }
    }
    else {
        // Address bus A + /CART
        A_BusAccess( addressBus, dataBus, writeLine, false, true );
    }

    if ( !writeLine ) {
        *data = MDR;
    }

}

uint8_t MainBusReadU8( MemoryAddress addressBus ) {
    uint8_t value;
    MemoryAccess( addressBus, &value, false );
    return value;
}

void MainBusWriteU8( MemoryAddress addressBus, uint8_t value ) {
    MemoryAccess( addressBus, &value, true );
}

// TODO - handle page/bank wrapping for the 16-bit read/writes
uint16_t MainBusReadU16( MemoryAddress addressBus ) {
    uint8_t lsb = MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint8_t msb = MainBusReadU8( addressBus );

    return ( ( (uint16_t) msb ) << 8 ) | (uint16_t)lsb;
}

void MainBusWriteU16( MemoryAddress addressBus, uint16_t value ) {
    MainBusWriteU8( addressBus, (uint8_t)( value & 0x00FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 8 ) & 0x00FF ) );
}

// TODO - handle page/bank wrapping for the 16-bit read/writes
uint32_t MainBusReadU24( MemoryAddress addressBus ) {
    uint32_t lsb = (uint32_t)MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint32_t midsb = (uint32_t)MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint32_t msb = (uint32_t)MainBusReadU8( addressBus );

    return ( msb << 16 ) | ( midsb << 8 ) | lsb;
}

void MainBusWriteU32( MemoryAddress addressBus, uint32_t value ) {
    MainBusWriteU8( addressBus, (uint8_t)( value & 0x000000FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 8 ) & 0x000000FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 16 ) & 0x000000FF ) );
}

#pragma endregion