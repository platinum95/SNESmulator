/*
*
*/

#include "cpu.h"

#include "dma.h"
#include "exec_compare.h"
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

#define CARRY_FLAG  0x1
#define ZERO_FLAG 0x2
#define INTERRUPT_FLAG 0x04
#define DECIMAL_FLAG 0x08
#define OVERFLOW_FLAG 0x40
#define NEGATIVE_FLAG 0x80
#define M_FLAG 0x20
#define X_FLAG 0x10

static bool inEmulationMode = 1;

static uint8_t p_register;

static uint16_t accumulator;
static uint16_t PC;
static uint16_t SP;
static uint8_t DBR;
static uint8_t PBR;
static uint16_t DP;
static uint16_t X, Y;
static uint8_t emulation_flag; //emulation flag, lowest bit only

static uint8_t MDR;

static uint32_t call_stack[30];
static uint8_t cs_counter = 0;

static uint8_t IRQpin;
static uint8_t InternalNMIFlag;

static bool inIRQHandler;
typedef struct InstructionEntry {
    void (*operation)(void);
    uint8_t bytes;
    uint8_t cycles;
} InstructionEntry;

static InstructionEntry instructions[ 0x100 ];

void cpuInitialise() {

    // TODO - get actual entry point addresses
    PC = 0xFF70;
    PBR = 0x00;
    SP = 0x01FF;//0x8000;
    DP = (uint16_t)0x00;
    p_register = 0x34;
    inEmulationMode = 0x01;
    emulation_flag = 0x01;
}

ExecutionState GetExecutionState() {
    ExecutionState state;
    state.PBR = PBR;
    state.PC = PC;
    state.A = accumulator;
    state.X = X;
    state.Y = Y;
    state.emulationMode = inEmulationMode ? 1 : 0;
    state.pRegister = p_register;
    state.DP = DP;
    state.DB = DBR;
    state.SP = SP;
    return state;
}

void IRQ( bool level ) {
    if ( IRQpin != level ) {
        IRQpin = level;
    }
}

// TODO - move
static uint8_t RDNMI;
static uint8_t NMITIMEN;
void NMI( bool start ) {
    // TODO - if NMI enabled after being disabled and flag hasn't been cleared, NMI should trigger again
    //      e.g.: NMI Enabled -> V-Blank, RDNMI Set, CPU goes to vector -> 
    //      CPU RTI -> RDNMI not read/cleared -> NMI disabled -> 
    //      NMI enabled before V-blank end -> CPU goes to vector again.

    if ( start ) {
        RDNMI |= 0x80;
        if ( NMITIMEN & 0x80 ) {
            InternalNMIFlag = 0x01;
        }
    }
    else {
        RDNMI &= ~0x80;
    }
    InternalNMIFlag = 0x01;
}

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
            InternalNMIFlag = true;
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
        }
    }
}

typedef enum Vectors {
    Vector_COP,
    Vector_BRK,
    Vector_ABORT,
    Vector_NMI,
    Vector_IRQ,
    Vector_RESET
} Vectors;

static inline uint16_t GetVectorValue( Vectors vector ) {
    if ( inEmulationMode ) {
        switch( vector ) {
            case Vector_COP:
                return 0xFFE0;
                break;
            case Vector_BRK:
                return 0xFFE6;
                break;
            case Vector_ABORT:
                return 0xFFE8;
                break;
            case Vector_NMI:
                return 0xFFEA;
                break;
            case Vector_IRQ:
                return 0xFFEE;
                break;
            case Vector_RESET:
                // ?
                return 0x0000;
                break;
            default:
                assert( false );
                break;
        }
    }
    else {
        switch( vector ) {
            case Vector_COP:
                return 0xFFF4;
                break;
            case Vector_BRK:
                return 0xFFFE;
                break;
            case Vector_ABORT:
                return 0xFFF8;
                break;
            case Vector_NMI:
                return 0xFFFA;
                break;
            case Vector_IRQ:
                return 0xFFFE;
                break;
            case Vector_RESET:
                return 0xFFFC;
                break;
            default:
                assert( false );
                break;
        }
    }
    assert( false );
    return 0x0000;
}

static inline uint8_t MainBusReadU8( MemoryAddress addressBus );

void executeIRQ();
void executeNMI();

// Used as the base address of the current operation
uint16_t currentOperationOffset;
uint16_t nextOperationOffset;
void cpuTick() {
    hTimerTick();
    // TODO - Maybe after instruction?
    if ( InternalNMIFlag ) {
        InternalNMIFlag = false;
        executeNMI();
    }
    else if ( !IRQpin && !inIRQHandler ) {
        executeIRQ( Vector_IRQ );
    }

    if ( !dmaTick() ) {
        currentOperationOffset = PC;
        uint8_t currentOpcode = MainBusReadU8( (MemoryAddress){ PBR, PC } );

#if 0
        static int counter = 0;
        counter++;
        if ( counter == 1 ) {
            start_comp();
        }
        uint8_t comparison = compare( GetExecutionState() );
        if ( comparison != Match &&
            ( comparison & ~( AccumulatorMismatch | PRegisterMismatch ) ) ){
            // Current max score - 45873
            printf( "Mismatch\n" );
        }

        printf( "\n%03i | %02x:\n", counter, currentOpcode );
#endif
        InstructionEntry *entry = &instructions[ currentOpcode ];
        nextOperationOffset = PC + entry->bytes;

        // Opcode consumed, inc PC for getting operand(s)
        ++PC;
        entry->operation();
        PC = nextOperationOffset;
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
                InternalNMIFlag = true;
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

static inline uint8_t MainBusReadU8( MemoryAddress addressBus ) {
    uint8_t value;
    MemoryAccess( addressBus, &value, false );
    return value;
}

static inline void MainBusWriteU8( MemoryAddress addressBus, uint8_t value ) {
    MemoryAccess( addressBus, &value, true );
}

// TODO - handle page/bank wrapping for the 16-bit read/writes
static inline uint16_t MainBusReadU16( MemoryAddress addressBus ) {
    uint8_t lsb = MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint8_t msb = MainBusReadU8( addressBus );

    return ( ( (uint16_t) msb ) << 8 ) | (uint16_t)lsb;
}

static inline void MainBusWriteU16( MemoryAddress addressBus, uint16_t value ) {
    MainBusWriteU8( addressBus, (uint8_t)( value & 0x00FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 8 ) & 0x00FF ) );
}

// TODO - handle page/bank wrapping for the 16-bit read/writes
static inline uint32_t MainBusReadU24( MemoryAddress addressBus ) {
    uint32_t lsb = (uint32_t)MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint32_t midsb = (uint32_t)MainBusReadU8( addressBus );
    ++addressBus.offset;
    uint32_t msb = (uint32_t)MainBusReadU8( addressBus );

    return ( msb << 16 ) | ( midsb << 8 ) | lsb;
}

static inline void MainBusWriteU32( MemoryAddress addressBus, uint32_t value ) {
    MainBusWriteU8( addressBus, (uint8_t)( value & 0x000000FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 8 ) & 0x000000FF ) );
    ++addressBus.offset;
    MainBusWriteU8( addressBus, (uint8_t)( ( value >> 16 ) & 0x000000FF ) );
}

#pragma region addressing_modes


// Hopefully RVO kicks in for these
static inline MemoryAddress GetBusAddressFromLong( uint32_t longAddress ) {
    return (MemoryAddress) {
        (uint8_t) ( ( longAddress >> 16 ) & 0xFF ),
        (uint16_t) ( longAddress & 0x00FFFF )
    };
}

static inline MemoryAddress GetBusAddress( uint8_t bank, uint16_t offset ) {
    return (MemoryAddress) { bank, offset };
}

static inline MemoryAddress GetDataBankAddress( uint16_t offset ) {
    return GetBusAddress( DBR, offset );
}

static inline MemoryAddress GetProgramBankAddress( uint16_t offset ) {
    return GetBusAddress( PBR, offset );
}

// TODO - Should be immediateU8
static inline uint8_t immediate() {
    return MainBusReadU8( GetProgramBankAddress( PC++ ) );
}

static inline uint16_t immediateU16() {
    uint16_t val = ( (uint16_t)immediate() );
    val |= ( (uint16_t)immediate() ) << 8;
    return val;
}

static inline uint32_t immediateU24() {
    uint32_t val = ( (uint32_t)immediate() );
    val |= ( (uint32_t)immediate() ) << 8;
    val |= ( (uint32_t)immediate() ) << 16;
    return val;
}

static inline int8_t relative() {
    return (int8_t)immediate();
}

static inline int16_t relativeLong() {
    return (int16_t)immediateU16();
}

static inline MemoryAddress direct( uint16_t offset ) {
    // TODO - switch between DP and ZP based on emulation flag
    return GetBusAddress( 0x00, DP + immediate() + offset );
}

static inline MemoryAddress directIndexedIndirect( uint16_t indexOffset ) {
    // TODO - should the offset be masked in 8-bit-register mode?
    return GetDataBankAddress( MainBusReadU16( direct( indexOffset ) ) );
}

static inline MemoryAddress directIndirect() {
    return directIndexedIndirect( 0 );
}

static inline MemoryAddress directIndexedXIndirect() {
    return directIndexedIndirect( X );
}

// Returns a host-mem pointer to the emulated indirect address
static inline MemoryAddress directIndirectLong() {
    return GetBusAddressFromLong( MainBusReadU24( direct( 0 ) ) );
}

static inline MemoryAddress directIndirectIndexedY() {
    return GetDataBankAddress( MainBusReadU16( direct( 0 ) ) + Y );
}

static inline MemoryAddress directIndirectIndexedYLong() {
    // TODO - not sure how to handle overflow here, ie does bank increment?
    MemoryAddress address = GetBusAddressFromLong( MainBusReadU24( direct( 0 ) ) );
    address.offset += Y;
    //address.offset |= 0x8000; // ?
    return address;
}

static inline uint16_t absoluteAsAddrOffset( uint16_t offset ) {
    return immediateU16() + offset;
}

static inline MemoryAddress absolute( uint16_t offset ) {
    return GetDataBankAddress( absoluteAsAddrOffset( offset ) );
}

static inline MemoryAddress absoluteIndexedX() {
    return absolute( X );
}

static inline MemoryAddress absoluteIndexedY() {
    return absolute( Y );
}

static inline MemoryAddress absoluteLongAsAddr( uint16_t offset ) {
    // TODO - not sure how to handle overflow here, ie does bank increment?
    MemoryAddress address = GetBusAddressFromLong( immediateU24() );
    address.offset += offset;
    return address;
}

static inline MemoryAddress absoluteLong( uint16_t offset ) {
    return absoluteLongAsAddr( offset );
}

static inline MemoryAddress absoluteLongIndexedX() {
    // TODO - not sure how to handle overflow here, ie does bank increment?
    return absoluteLong( X );
}

static inline MemoryAddress absoluteIndirect() {
    return GetDataBankAddress( immediateU16() );
}

static inline MemoryAddress absoluteIndexedXIndirect() {
    return GetDataBankAddress( immediateU16() + X );
}

static inline MemoryAddress stackRelative() {
    // TODO - should the relative part be signed?
    return GetBusAddress( 0x00, SP + immediate() );
}

static inline MemoryAddress stackRelativeIndirectIndexedY() {
    // TODO - verify
    return GetDataBankAddress( MainBusReadU16( stackRelative() ) + Y );
}

#pragma endregion


#pragma region INSTRUCTIONS

#pragma region stack_access
// TODO - stack accessors should handle emulation mode

static inline void pushU8( uint8_t value ) {
    MainBusWriteU8( GetBusAddress( 0x00, SP-- ), value );
}

static inline void pushU16( uint16_t val ) {
    pushU8( (uint8_t)( ( val >> 8 ) & 0x00FF ) );
    pushU8( (uint8_t)( val & 0x00FF ) );
}

static inline uint8_t popU8() {
    return MainBusReadU8( GetBusAddress( 0x00, ++SP ) );
}

static inline uint16_t popU16() {
    uint16_t val = (uint16_t) popU8();
    val |= ( (uint16_t) popU8() ) << 8;
    return val;
}

#pragma endregion

#pragma region ADC
int BCD_ADD_NYBBLE(uint8_t *reg, uint8_t toAdd) {
     uint8_t reg_val = *reg;
     uint8_t added = reg_val + toAdd;
     uint8_t carry = added & 0x10;
     if (added > 9) {
         carry = 1;
         added += 0x06;
     }
     added &= 0x0F;
     *reg = added;
     return carry;
}

static inline void ADC8( uint8_t O1 ) {
    bool decimal = ( p_register & DECIMAL_FLAG );
    uint8_t a = accumulator, b, r;

    uint8_t zero = 0x00;
    uint8_t carry = 0x00;
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;

    if ( decimal ) {
        uint8_t carryVal = ( p_register & CARRY_FLAG ) ? 0x01 : 0x00;
        uint16_t sum = 0;
        for ( int i = 0; i < 2; ++i ) {
            uint8_t reg = ( accumulator & ( 0x000F << ( i * 4 ) ) ) >> ( i * 4 );
            uint8_t toAddy = ( O1 & ( 0x000F << ( i * 4 ) ) ) >> ( i * 4 );
            carryVal = BCD_ADD_NYBBLE( &reg, toAddy + carryVal );
            sum |= reg << ( i * 4 );
        }
        a = b = accumulator;
        r = accumulator = sum;
        carry = carryVal ? CARRY_FLAG : carry;
    }
    else {
        const uint16_t rhs16 = (uint16_t) ( O1 );
        const uint16_t lhs16 = rhs16 + ( ( CARRY_FLAG & p_register ) ? 0x01 : 0x00 ) + ( accumulator & 0x00ff );
        b = O1;
        //Check for carry
        if ( lhs16 & 0xFF00 ) {
            carry = CARRY_FLAG;
        }

        accumulator &= 0xff00;// lhs16; // accumulator + toAdd + (CARRY_FLAG & p_register);
        accumulator |= ( lhs16 & 0x00ff );
        r = accumulator;
    }

    zero = ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : zero;
    negative = ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : negative;
    if ( ( ( a & 0x80 ) == ( b & 0x80 ) ) && ( ( r & 0x80 ) != ( a & 0x80 ) ) ) {
        overflow = OVERFLOW_FLAG;
    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | CARRY_FLAG | ZERO_FLAG | OVERFLOW_FLAG ) )
        | negative | zero | carry | overflow;
}

static inline void ADC16( uint16_t O1 ) {
    // TODO - rewrite all this
    bool decimal = ( p_register & DECIMAL_FLAG );
    uint8_t zero = 0x00;
    uint8_t carry = 0x00;
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;
    //--------------------------------------------------------------------------
    uint8_t d_on = p_register & DECIMAL_FLAG;
    uint16_t a = accumulator, b, r;
    uint16_t toAdd = O1;
    if (d_on > 0) {
        uint8_t carryVal = p_register & CARRY_FLAG;
        uint16_t sum = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t reg = (accumulator & (0x000F << (i * 4))) >> i * 4;
            uint8_t toAddy = (toAdd & (0x000F << (i * 4))) >> i * 4;
            carryVal = BCD_ADD_NYBBLE(&reg, toAddy + carryVal);
            uint16_t val = reg;
            val <<= i * 4;
            sum |= val;
        }
        a = b = accumulator;
        r = accumulator = sum;
        //set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
        if ( carryVal )
            carry = CARRY_FLAG;
    }
    else {
        b = toAdd;
        accumulator = accumulator + toAdd + (1 & p_register);
        r = accumulator;
    }
    if(((a & 0x8000) == (b & 0x8000)) && ((r & 0x8000) != (a & 0x8000)))
        overflow = OVERFLOW_FLAG;
    zero = ( accumulator == 0 ) ? ZERO_FLAG : zero;
    negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : negative;

     p_register = ( p_register & ~( NEGATIVE_FLAG | CARRY_FLAG | ZERO_FLAG | OVERFLOW_FLAG ) )
           | negative | zero | carry | overflow;
}

static inline void ADCMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        // 8-bit mode
        ADC8( MainBusReadU8( address ) );
    }
    else {
        // 16-bit mode
        ADC16( MainBusReadU16( address ) );
    }
}

//ADC(_dp_, X)    61    DP Indexed Indirect, X    NV� - ZC    2
void f61_ADC(){
     ADCMem( directIndexedXIndirect() );
}

//ADC sr, S    63    Stack Relative    NV� - ZC    2
void f63_ADC(){
     ADCMem( stackRelative() );
}

//ADC dp    65    Direct Page    NV� - ZC    2
void f65_ADC(){
     ADCMem( direct( 0 ) );
}

//ADC[_dp_]    67    DP Indirect Long    NV� - ZC    2
void f67_ADC(){
     ADCMem( directIndirectLong() );
}

//ADC #const    69    Immediate    NV� - ZC    2/3
void f69_ADC(){
    // TODO - maybe immediateMFlag() or something?
    if ( p_register & M_FLAG ) {
        ADC8( immediate() );
    }
    else {
        ADC16( immediateU16() );
        ++nextOperationOffset;
    }
}

//ADC addr    6D    Absolute    NV� - ZC    3
void f6D_ADC(){
    ADCMem( absolute( 0 ) );
}

//ADC long    6F    Absolute Long    NV� - ZC    4
void f6F_ADC(){
    ADCMem( absoluteLong( 0 ) );
}

//ADC(dp), Y    71    DP Indirect Indexed, Y    NV� - ZC    2
void f71_ADC(){
    ADCMem( directIndirectIndexedY() );
}

//ADC(_dp_)    72    DP Indirect    NV� - ZC    2
void f72_ADC(){
    ADCMem( directIndirect() );
}

//ADC(_sr_, S), Y    73    SR Indirect Indexed, Y    NV� - ZC    2
void f73_ADC(){
    ADCMem( stackRelativeIndirectIndexedY() );
}

//ADC dp, X    75    DP Indexed, X    NV� - ZC    2
void f75_ADC(){
    ADCMem( direct( X ) );
}

//ADC[_dp_], Y    77    DP Indirect Long Indexed, Y    NV� - ZC    2
void f77_ADC(){
    // TODO - verify
    ADCMem( directIndirectIndexedYLong() );
}

//ADC addr, Y    79    Absolute Indexed, Y    NV� - ZC    3
void f79_ADC(){
    ADCMem( absoluteIndexedY() );
}

//ADC addr, X    7D    Absolute Indexed, X    NV� - ZC    3
void f7D_ADC(){
    ADCMem( absoluteIndexedX() );
}

//ADC long, X    7F    Absolute Long Indexed, X    NV� - ZC    4
void f7F_ADC(){
    ADCMem( absoluteLongIndexedX() );
}

#pragma endregion

#pragma region AND
static inline void AND8( uint8_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;

    accumulator = ( accumulator & 0XFF00 ) | ( accumulator & O1 );
    negative = ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : negative;
    zero = ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | zero | negative;
}

static inline void AND16( uint16_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;

    accumulator &= O1;
    negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( accumulator == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | zero | negative;
}

static inline void ANDMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        AND8( MainBusReadU8( address ) );
    }
    else {
        AND16( MainBusReadU16( address ) );
    }
}

//AND(_dp, _X)    21    DP Indexed Indirect, X    N��Z - 2
void f21_AND(){
    ANDMem( directIndexedXIndirect() );
}

//AND sr, S    23    Stack Relative    N��Z - 2
void f23_AND(){
    ANDMem( stackRelative() );
}

////AND dp    25    Direct Page    N��Z - 2
void f25_AND(){
    ANDMem( direct( 0 ) );
}

////AND[_dp_]    27    DP Indirect Long    N��Z - 2
void f27_AND(){
    ANDMem( directIndirectLong() );
}

////AND #const    29    Immediate    N��Z - 2
void f29_AND(){
    if ( p_register & M_FLAG ) {
        AND8( immediate() );
    }
    else {
        AND16( immediateU16() );
        ++nextOperationOffset;
    }
}

////AND addr    2D    Absolute    N��Z - 3
void f2D_AND(){
    ANDMem( absolute( 0 ) );
}

////AND long    2F    Absolute Long    N��Z - 4
void f2F_AND(){
    ANDMem( absoluteLong( 0 ) );
}

////AND(_dp_), Y    31    DP Indirect Indexed, Y    N��Z - 2
void f31_AND(){
    ANDMem( directIndirectIndexedY( 0 ) );
}

////AND(_dp_)    32    DP Indirect    N��Z - 2
void f32_AND(){
    ANDMem( directIndirect() );
}

////AND(_sr_, S), Y    33    SR Indirect Indexed, Y    N��Z - 2
void f33_AND(){
    ANDMem( stackRelativeIndirectIndexedY() );
}

////AND dp, X    35    DP Indexed, X    N��Z - 2
void f35_AND(){
    ANDMem( direct( X ) );
}

////AND[_dp_], Y    37    DP Indirect Long Indexed, Y    N��Z - 2
void f37_AND(){
    ANDMem( directIndirectIndexedYLong() );
}

////AND addr, Y    39    Absolute Indexed, Y    N��Z - 3
void f39_AND(){
    ANDMem( absoluteIndexedY() );
}

////AND addr, X    3D    Absolute Indexed, X    N��Z - 3
void f3D_AND(){
    ANDMem( absoluteIndexedX() );
}

////AND long, X    3F    Absolute Long Indexed, X    N��Z - 4
void f3F_AND(){
    ANDMem( absoluteLongIndexedX() );
}

#pragma endregion

#pragma region ASL

void ASL( uint8_t *O1 ){
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;
    uint16_t carry = 0x00;

    if ( p_register & M_FLAG ) {
        carry = *O1 & 0x80;
        *O1 = ( *O1 << 1 ) & 0xFE;
        zero = ( *O1 == 0 ) ? ZERO_FLAG : zero;
        negative = ( *O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    }
    else {
        uint16_t val = readU16( O1 );
        carry = val & 0x8000;
        val = val << 1;
        zero = ( val == 0 ) ? ZERO_FLAG : zero;
        negative = ( val & 0x8000 ) ? NEGATIVE_FLAG : negative;
        storeU16( O1, val );
    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | CARRY_FLAG ) )
        | negative | zero | carry;
}

static inline uint8_t ASL8( uint8_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;
    uint8_t carry = 0x00;

    carry = O1 & 0x80;
    uint8_t result = ( O1 << 1 ) & 0xFE;
    zero = ( result == 0 ) ? ZERO_FLAG : zero;
    negative = ( result & 0x80 ) ? NEGATIVE_FLAG : negative;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | CARRY_FLAG ) )
        | negative | zero | carry;

    return result;
}

static inline uint16_t ASL16( uint16_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;
    uint8_t carry = 0x00;

    uint16_t val = O1;
    carry = ( val & 0x8000 ) ? CARRY_FLAG : carry;
    val = val << 1;
    zero = ( val == 0 ) ? ZERO_FLAG : zero;
    negative = ( val & 0x8000 ) ? NEGATIVE_FLAG : negative;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | CARRY_FLAG ) )
        | negative | zero | carry;

    return val;
}

static inline void ASLMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, ASL8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, ASL8( MainBusReadU16( address ) ) );
    }
}

////ASL dp    6    Direct Page    N��ZC    2
void f06_ASL(){
    ASLMem( direct( 0 ) );
}

////ASL A    0A    Accumulator    N��ZC    1
void f0A_ASL(){
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | ASL8( (uint8_t)accumulator );
    }
    else {
        accumulator = ASL16( accumulator );
    }
}

////ASL addr    0E    Absolute    N��ZC    3
void f0E_ASL(){
    ASLMem( absolute( 0 ) );
}

////ASL dp, X    16    DP Indexed, X    N��ZC    2
void f16_ASL(){
    ASLMem( direct( X ) );
}

////ASL addr, X    1E    Absolute Indexed, X    N��ZC    3
void f1E_ASL(){
    ASLMem( absoluteIndexedX() );
}
#pragma endregion

static inline void BranchRelativeOnCondition( bool condition ) {
    int8_t offset = (int8_t) immediate();
    if ( condition ){
        nextOperationOffset += offset;
    }
}

////BCC nearlabel    90    Program Counter Relative        2
void f90_BCC(){
    BranchRelativeOnCondition( !( p_register & CARRY_FLAG ) );
}

////BCS nearlabel    B0    Program Counter Relative        2
void fB0_BCS(){
    BranchRelativeOnCondition( CARRY_FLAG & p_register );
}

////BEQ nearlabel    F0    Program Counter Relative        2
void fF0_BEQ(){
    BranchRelativeOnCondition( p_register & ZERO_FLAG );
}


////BMI nearlabel    30    Program Counter Relative        2
void f30_BMI(){
    BranchRelativeOnCondition( p_register & NEGATIVE_FLAG );
}

////BNE nearlabel    D0    Program Counter Relative        2
void fD0_BNE(){
    BranchRelativeOnCondition( !( p_register & ZERO_FLAG ) );
}

////BPL nearlabel    10    Program Counter Relative        2
void f10_BPL(){
    BranchRelativeOnCondition( !( p_register & NEGATIVE_FLAG ) );
}

////BRA nearlabel    80    Program Counter Relative        2
void f80_BRA(){
    BranchRelativeOnCondition( true );
}

////BVC nearlabel    50    Program Counter Relative        2
void f50_BVC(){
    BranchRelativeOnCondition( !( p_register & OVERFLOW_FLAG ) );
}

////BVS nearlabel    70    Program Counter Relative        2
void f70_BVS(){
    BranchRelativeOnCondition( p_register & OVERFLOW_FLAG );
}

////BRK    0    Stack / Interrupt    � - DI�    28
void f00_BRK(){
    executeIRQ( Vector_BRK );
}

////BRL label    82    Program Counter Relative Long        3
void f82_BRL(){
    int16_t offset = (int16_t) immediateU16();
    nextOperationOffset += offset;
}


#pragma region BIT

static inline void BIT8( uint8_t O1 ){
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;
    uint8_t zero = 0x00;

    uint8_t result = (uint8_t) ( accumulator & 0x00FF ) & O1;
    zero = ( result == 0 ) ? ZERO_FLAG : zero;
    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    overflow = ( O1 & 0x40 ) ? OVERFLOW_FLAG : overflow;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | OVERFLOW_FLAG  ) )
        | zero | negative | overflow;
}

static inline void BIT16( uint16_t O1 ){
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;
    uint8_t zero = 0x00;

    uint16_t result = accumulator & O1;
    zero = ( result == 0 ) ? ZERO_FLAG : zero;
    negative = ( O1 & 0x8000 ) ? NEGATIVE_FLAG : negative;
    overflow = ( O1 & 0x4000 ) ? OVERFLOW_FLAG : overflow;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | OVERFLOW_FLAG  ) )
        | zero | negative | overflow;
}

static inline void BITMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        BIT8( MainBusReadU8( address ) );
    }
    else {
        BIT16( MainBusReadU16( address ) );
    }
}

////BIT dp    24    Direct Page    NV� - Z - 2
void f24_BIT(){
    BITMem( direct( 0 ) );
}

////BIT addr    2C    Absolute    NV� - Z - 3
void f2C_BIT(){
    BITMem( absolute( 0 ) );
}

////BIT dp, X    34    DP Indexed, X    NV� - Z - 2
void f34_BIT(){
    BITMem( direct( X ) );
}

////BIT addr, X    3C    Absolute Indexed, X    NV� - Z - 3
void f3C_BIT(){
    BITMem( absoluteIndexedX() );
}

////BIT #const    89    Immediate    ��Z - 2
void f89_BIT(){
    // TODO ensure N and V are unaffected
    uint16_t pRegisterPersistent = p_register & ( NEGATIVE_FLAG | OVERFLOW_FLAG );
    if ( p_register & M_FLAG ) {
        BIT8( immediate() );
    }
    else {
        BIT16( immediateU16() );
        ++nextOperationOffset;
    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | OVERFLOW_FLAG ) ) | pRegisterPersistent;
}
#pragma endregion

////CLC    18    Implied    �� - C    1
void f18_CLC(){
    p_register &= ~CARRY_FLAG;
}

////CLD    D8    Implied    � - D�    1
void fD8_CLD(){
    p_register &= ~DECIMAL_FLAG;
}

////CLI    58    Implied    ��I�    1
void f58_CLI(){
    p_register &= ~INTERRUPT_FLAG;
}

////CLV    B8    Implied    #NAME ? 1
void fB8_CLV(){
    p_register &= ~OVERFLOW_FLAG;
}

#pragma region cmp

static inline void CMP8( uint8_t registerValue, uint8_t O1 ) {
    uint16_t zero = 0x00;
    uint16_t negative = 0x00;
    uint16_t carry = 0x00;

    uint8_t regU8 = (uint8_t) ( registerValue & 0x00FF );
    uint8_t O1U8 = O1;
    uint8_t res = regU8 - O1U8;
    zero = ( res == 0 ) ? ZERO_FLAG : 0x00;
    negative = ( res & 0x80 ) ? NEGATIVE_FLAG : 0x00;
    carry = ( regU8 >= O1U8 ) ? CARRY_FLAG : 0x00;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;
}
static inline void CMP16( uint16_t registerValue, uint16_t O1 ) {
    uint16_t zero = 0x00;
    uint16_t negative = 0x00;
    uint16_t carry = 0x00;
    uint16_t O1U16 = O1;
    uint16_t res = registerValue - O1U16;
    zero = ( res == 0 ) ? ZERO_FLAG : 0x00;
    negative = ( res & 0x8000 ) ? NEGATIVE_FLAG : 0x00;
    carry = ( registerValue >= O1U16 ) ? CARRY_FLAG : 0x00;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;
}
static inline void CMPMem( uint16_t registerValue, MemoryAddress address, uint8_t mask ) {
    if ( p_register & mask ) {
        CMP8( (uint8_t)registerValue, MainBusReadU8( address ) );
    }
    else {
        CMP16( registerValue, MainBusReadU16( address ) );
    }
}

////CMP(_dp, _X)    C1    DP Indexed Indirect, X    N��ZC    2
void fC1_CMP(){
    CMPMem( accumulator, directIndexedXIndirect(), M_FLAG );
}

////CMP sr, S    C3    Stack Relative    N��ZC    2
void fC3_CMP(){
    CMPMem( accumulator, stackRelative(), M_FLAG );
}

////CMP dp    C5    Direct Page    N��ZC    2
void fC5_CMP(){
    CMPMem( accumulator, direct( 0 ), M_FLAG );
}

////CMP[_dp_]    C7    DP Indirect Long    N��ZC    2
void fC7_CMP(){
    CMPMem( accumulator, directIndirectLong(), M_FLAG );
}

////CMP #const    C9    Immediate    N��ZC    2
void fC9_CMP(){
    if ( p_register & M_FLAG ) {
        CMP8( (uint8_t)accumulator, immediate() );
    }
    else {
        CMP16( accumulator, immediateU16() );
        ++nextOperationOffset;
    }
}

////CMP addr    CD    Absolute    N��ZC    3
void fCD_CMP(){
    CMPMem( accumulator, absolute( 0 ), M_FLAG );
}

////CMP long    CF    Absolute Long    N��ZC    4
void fCF_CMP(){
    CMPMem( accumulator, absoluteLong( 0 ), M_FLAG );
}

////CMP(_dp_), Y    D1    DP Indirect Indexed, Y    N��ZC    2
void fD1_CMP(){
    CMPMem( accumulator, directIndirectIndexedY(), M_FLAG );
}

////CMP(_dp_)    D2    DP Indirect    N��ZC    2
void fD2_CMP(){
    CMPMem( accumulator, directIndirect(), M_FLAG );
}

////CMP(_sr_, S), Y    D3    SR Indirect Indexed, Y    N��ZC    2
void fD3_CMP(){
    CMPMem( accumulator, stackRelativeIndirectIndexedY(), M_FLAG );
}

////CMP dp, X    D5    DP Indexed, X    N��ZC    2
void fD5_CMP(){
    CMPMem( accumulator, direct( X ), M_FLAG );
}

////CMP[_dp_], Y    D7    DP Indirect Long Indexed, Y    N��ZC    2
void fD7_CMP(){
    CMPMem( accumulator, directIndirectIndexedYLong(), M_FLAG );
}

////CMP addr, Y    D9    Absolute Indexed, Y    N��ZC    3
void fD9_CMP(){
    CMPMem( accumulator, absoluteIndexedY(), M_FLAG );
}

////CMP addr, X    DD    Absolute Indexed, X    N��ZC    3
void fDD_CMP(){
    CMPMem( accumulator, absoluteIndexedX(), M_FLAG );
}

////CMP long, X    DF    Absolute Long Indexed, X    N��ZC    4
void fDF_CMP(){
    CMPMem( accumulator, absoluteLongIndexedX(), M_FLAG );
}

////CPX #const    E0    Immediate    N��ZC    210
void fE0_CPX(){
    if ( p_register & X_FLAG ) {
        CMP16( (uint8_t)X, immediate() );
    }
    else {
        CMP16( X, immediateU16() );
        ++nextOperationOffset;
    }
}

////CPX dp    E4    Direct Page    N��ZC    2
void fE4_CPX(){
    CMPMem( X, direct( 0 ), X_FLAG );
}

////CPX addr    EC    Absolute    N��ZC    3
void fEC_CPX(){
    CMPMem( X, absolute( 0 ), X_FLAG );
}

////CPY #const    C0    Immediate    N��ZC    2
void fC0_CPY(){
    if ( p_register & X_FLAG ) {
        CMP16( (uint8_t)Y, immediate() );
    }
    else {
        CMP16( Y, immediateU16() );
        ++nextOperationOffset;
    }
}

////CPY dp    C4    Direct Page    N��ZC    2
void fC4_CPY(){
    CMPMem( Y, direct( 0 ), X_FLAG );
}

////CPY addr    CC    Absolute    N��ZC    3
void fCC_CPY(){
    CMPMem( Y, absolute( 0 ), X_FLAG );
}
#pragma endregion

////COP const    2    Stack / Interrupt    � - DI�    2
void f02_COP() {
    executeIRQ( Vector_COP );
}

#pragma region INC_DEC

static inline uint8_t DEC8( uint8_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;

    --O1;
    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}

static inline uint16_t DEC16( uint16_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;
    --O1;
    negative = ( O1 & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}

static inline void DECMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, DEC8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, DEC16( MainBusReadU16( address ) ) );
    }
}

////DEC A    3A    Accumulator    N��Z - 1
void f3A_DEC(){
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | DEC8( (uint8_t)accumulator );
    }
    else {
        accumulator = DEC16( accumulator );
    }
}

////DEC dp    C6    Direct Page    N��Z - 2
void fC6_DEC(){
    DECMem( direct( 0 ) );
}

////DEC addr    CE    Absolute    N��Z - 3
void fCE_DEC(){
    DECMem( absolute( 0 ) );
}

////DEC dp, X    D6    DP Indexed, X    N��Z - 2
void fD6_DEC(){
    DECMem( direct( X ) );
}

////DEC addr, X    DE    Absolute Indexed, X    N��Z - 3
void fDE_DEC(){
    DECMem( absoluteIndexedX() );
}

////DEX    CA    Implied    N��Z - 1
void fCA_DEX(){
    if ( p_register & X_FLAG ) {
        X = ( X & 0xFF00 ) | DEC8( (uint8_t)X );
    }
    else {
        X = DEC16( X );
    }
}

////DEY    88    Implied    N��Z - 1
void f88_DEY(){
    if ( p_register & X_FLAG ) {
        Y = ( Y & 0xFF00 ) | DEC8( (uint8_t)Y );
    }
    else {
        Y = DEC16( Y );
    }
}

static inline uint8_t INC8( uint8_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;

    ++O1;
    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}

static inline uint16_t INC16( uint16_t O1 ) {
    uint8_t negative = 0x00;
    uint8_t zero = 0x00;
    ++O1;
    negative = ( O1 & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}

static inline void INCMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, INC8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, INC16( MainBusReadU16( address ) ) );
    }
}

////INC A    1A    Accumulator    N��Z - 1
void f1A_INC() {
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | INC8( (uint8_t)accumulator );
    }
    else {
        accumulator = INC16( accumulator );
    }
}

////INC dp    E6    Direct Page    N��Z - 2
void fE6_INC() {
    INCMem( direct( 0 ) );
}

////INC addr    EE    Absolute    N��Z - 3
void fEE_INC() {
    INCMem( absolute( 0 ) );
}

////INC dp, X    F6    DP Indexed, X    N��Z - 2
void fF6_INC() {
    INCMem( direct( X ) );
}

////INC addr, X    FE    Absolute Indexed, X    N��Z - 3
void fFE_INC() {
    INCMem( absoluteIndexedX() );
}

////INX    E8    Implied    N��Z - 1
void fE8_INX() {
    if ( p_register & X_FLAG ) {
        X = ( X & 0xFF00 ) | INC8( (uint8_t)X );
    }
    else {
        X = INC16( X );
    }
}

////INY    C8    Implied    N��Z - 1
void fC8_INY() {
    if ( p_register & X_FLAG ) {
        Y = ( Y & 0xFF00 ) | INC8( (uint8_t)Y );
    }
    else {
        Y = INC16( Y );
    }
}
#pragma endregion

#pragma region EOR

static inline void EOR8( uint8_t O1 ) {
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;

    uint8_t result = ( (uint8_t) ( accumulator & 0x00FF ) ) ^ O1;
    negative = ( result & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( result == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;
}

static inline void EOR16( uint16_t O1 ) {
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;
    uint16_t result = accumulator ^ O1;
    negative = ( result & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( result == 0x00 ) ? ZERO_FLAG : negative;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;
}

static inline void EORMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        EOR8( MainBusReadU8( address ) );
    }
    else {
        EOR16( MainBusReadU16( address ) );
    }
}

////EOR(_dp, _X)    41    DP Indexed Indirect, X    N��Z - 2
void f41_EOR(){
    EORMem( directIndexedXIndirect() );
}

////EOR sr, S    43    Stack Relative    N��Z - 2
void f43_EOR(){
    EORMem( stackRelative() );
}

////EOR dp    45    Direct Page    N��Z - 2
void f45_EOR(){
    EORMem( direct( 0 ) );
}

////EOR[_dp_]    47    DP Indirect Long    N��Z - 2
void f47_EOR(){
    EORMem( directIndirectLong() );
}

////EOR #const    49    Immediate    N��Z - 2
void f49_EOR(){
    if ( p_register & M_FLAG ) {
        EOR8( immediate() );
    }
    else {
        EOR16( immediateU16() );
        ++nextOperationOffset;
    }
}

////EOR addr    4D    Absolute    N��Z - 3
void f4D_EOR(){
    EORMem( absolute( 0 ) );
}

////EOR long    4F    Absolute Long    N��Z - 4
void f4F_EOR(){
    EORMem( absoluteLong( 0 ) );
}

////EOR(_dp_), Y    51    DP Indirect Indexed, Y    N��Z - 2
void f51_EOR(){
    EORMem( directIndirectIndexedY( 0 ) );
}

////EOR(_dp_)    52    DP Indirect    N��Z - 2
void f52_EOR(){
    EORMem( directIndirect() );
}

////EOR(_sr_, S), Y    53    SR Indirect Indexed, Y    N��Z - 2
void f53_EOR(){
    EORMem( stackRelativeIndirectIndexedY() );
}

////EOR dp, X    55    DP Indexed, X    N��Z - 2
void f55_EOR(){
    EORMem( direct( X ) );
}

////EOR[_dp_], Y    57    DP Indirect Long Indexed, Y    N��Z - 2
void f57_EOR(){
    EORMem( directIndirectIndexedYLong() );
}

////EOR addr, Y    59    Absolute Indexed, Y    N��Z - 3
void f59_EOR(){
    EORMem( absoluteIndexedY() );
}

////EOR addr, X    5D    Absolute Indexed, X    N��Z - 3
void f5D_EOR(){
    EORMem( absoluteIndexedX() );
}

////EOR long, X    5F    Absolute Long Indexed, X    N��Z - 4
void f5F_EOR(){
    EORMem( absoluteLongIndexedX() );
}
#pragma endregion

#pragma region JMP

static inline void JMP( uint16_t O1 ) {
    nextOperationOffset = O1;
}
static inline void JMPMem( MemoryAddress address ){
    JMP( MainBusReadU16( address ) );
}

static inline void JMPL( uint32_t O1 ) {
    MemoryAddress address = GetBusAddressFromLong( O1 );
    PBR = address.bank;
    nextOperationOffset = address.offset;
}
static inline void JMPLMem( MemoryAddress address ) {
    JMPL( MainBusReadU24( address ) );
}

////JMP addr    4C    Absolute        3
void f4C_JMP(){
    JMP( immediateU16() );
}

////JMP long    5C    Absolute Long        4
void f5C_JMP(){
    JMPL( immediateU24() );
}

////JMP(_addr_)    6C    Absolute Indirect        3
void f6C_JMP(){
    JMPMem( absoluteIndirect() );
}

////JMP(_addr, X_)    7C    Absolute Indexed Indirect        3
void f7C_JMP(){
    // TODO - verify
    JMPMem( absoluteIndexedXIndirect() );
}

////JMP[addr]    DC    Absolute Indirect Long        3
void fDC_JMP(){
    // TODO - verify
    JMPLMem( absoluteIndirect() );
}

static inline void JSR( uint16_t O1 ) {
    pushU16( nextOperationOffset - 1 );
    JMP( O1 );
}

////JSR addr    20    Absolute        3
void f20_JSR(){
    JSR( immediateU16() );
}

////JSR or JSL long    22    Absolute Long        4
void f22_JSR(){
    pushU8( PBR );
    pushU16( nextOperationOffset - 1 );
    JMPL( immediateU24() );
}

////JSR(addr, X))    FC    Absolute Indexed Indirect        3
void fFC_JSR(){
    // Not sure if this should be dereferenced
    JSR( MainBusReadU16( absoluteIndexedXIndirect() ) );
}
#pragma endregion

#pragma region LD

static inline uint8_t LD8( uint8_t O1 ) {
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;

    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}
static inline uint16_t LD16( uint16_t O1 ) {
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;

    negative = ( O1 & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;

    return O1;
}

static inline void LDA8( uint8_t O1 ) {
    accumulator = ( accumulator & 0xFF00 ) | (uint16_t) LD8( O1 );
}
static inline void LDA16( uint16_t O1 ) {
    accumulator = LD16( O1 );
}
static inline void LDAMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        LDA8( MainBusReadU8( address ) );
    }
    else {
        LDA16( MainBusReadU16( address ) );
    }
}

static inline void LDX8( uint8_t O1 ) {
    X = ( X & 0xFF00 ) | (uint16_t) LD8( O1 );
}
static inline void LDX16( uint16_t O1 ) {
    X = LD16( O1 );
}
static inline void LDXMem( MemoryAddress address ) {
    if ( p_register & X_FLAG ) {
        LDX8( MainBusReadU8( address ) );
    }
    else {
        LDX16( MainBusReadU16( address ) );
    }
}

static inline void LDY8( uint8_t O1 ) {
    Y = ( Y & 0xFF00 ) | (uint16_t) LD8( O1 );
}
static inline void LDY16( uint16_t O1 ) {
    Y = LD16( O1 );
}
static inline void LDYMem( MemoryAddress address ) {
    if ( p_register & X_FLAG ) {
        LDY8( MainBusReadU8( address ) );
    }
    else {
        LDY16( MainBusReadU16( address ) );
    }
}


////LDA(_dp, _X)    A1    DP Indexed Indirect, X    N��Z - 2
void fA1_LDA(){
    LDAMem( directIndexedXIndirect() );
}

////LDA sr, S    A3    Stack Relative    N��Z - 2
void fA3_LDA(){
    LDAMem( stackRelative() );
}

////LDA dp    A5    Direct Page    N��Z - 2
void fA5_LDA(){
    LDAMem( direct( 0 ) );
}

////LDA[_dp_]    A7    DP Indirect Long    N��Z - 2
void fA7_LDA(){
    LDAMem( directIndirectLong() );
}

////LDA #const    A9    Immediate    N��Z - 2
void fA9_LDA(){
    if ( p_register & M_FLAG ) {
        LDA8( immediate() );
    }
    else {
        LDA16( immediateU16() );
        ++nextOperationOffset;
    }
}

////LDA addr    AD    Absolute    N��Z - 3
void fAD_LDA(){
    LDAMem( absolute( 0 ) );
}

////LDA long    AF    Absolute Long    N��Z - 4
void fAF_LDA(){
    LDAMem( absoluteLong( 0 ) );
}

////LDA(_dp_), Y    B1    DP Indirect Indexed, Y    N��Z - 2
void fB1_LDA(){
    LDAMem( directIndirectIndexedY( 0 ) );
}

////LDA(_dp_)    B2    DP Indirect    N��Z - 2
void fB2_LDA(){
    LDAMem( directIndirect() );
}

////LDA(_sr_, S), Y    B3    SR Indirect Indexed, Y    N��Z - 2
void fB3_LDA(){
    LDAMem( stackRelativeIndirectIndexedY() );
}

////LDA dp, X    B5    DP Indexed, X    N��Z - 2
void fB5_LDA(){
    LDAMem( direct( X ) );
}

////LDA[_dp_], Y    B7    DP Indirect Long Indexed, Y    N��Z - 2
void fB7_LDA(){
    LDAMem( directIndirectIndexedYLong() );
}

////LDA addr, Y    B9    Absolute Indexed, Y    N��Z - 3
void fB9_LDA(){
    LDAMem( absoluteIndexedY() );
}

////LDA addr, X    BD    Absolute Indexed, X    N��Z - 3
void fBD_LDA(){
    LDAMem( absoluteIndexedX() );
}

////LDA long, X    BF    Absolute Long Indexed, X    N��Z - 4
void fBF_LDA(){
    LDAMem( absoluteLongIndexedX() );
}

////LDX #const    A2    Immediate    N��Z - 212
void fA2_LDX(){
    if ( p_register & X_FLAG ) {
        LDX8( immediate() );
    }
    else {
        LDX16( immediateU16() );
        ++nextOperationOffset;
    }
}

////LDX dp    A6    Direct Page    N��Z - 2
void fA6_LDX(){
    LDXMem( direct( 0 ) );
}

////LDX addr    AE    Absolute    N��Z - 3
void fAE_LDX(){
    LDXMem( absolute( 0 ) );
}

////LDX dp, Y    B6    DP Indexed, Y    N��Z - 2
void fB6_LDX(){
    LDXMem( direct( Y ) );
}

////LDX addr, Y    BE    Absolute Indexed, Y    N��Z - 3
void fBE_LDX(){
    LDXMem( absoluteIndexedY() );
}

////LDY #const    A0    Immediate    N��Z - 2
void fA0_LDY(){
    if ( p_register & X_FLAG ) {
        LDY8( immediate() );
    }
    else {
        LDY16( immediateU16() );
        ++nextOperationOffset;
    }
}

////LDY dp    A4    Direct Page    N��Z - 2
void fA4_LDY(){
    LDYMem( direct( 0 ) );
}

////LDY addr    AC    Absolute    N��Z - 3
void fAC_LDY(){
    LDYMem( absolute( 0 ) );
}

////LDY dp, X    B4    DP Indexed, X    N��Z - 2
void fB4_LDY(){
    LDYMem( direct( X ) );
}

////LDY addr, X    BC    Absolute Indexed, X    N��Z - 3
void fBC_LDY(){
    LDYMem( absoluteIndexedX() );
}
#pragma endregion

#pragma region LSR

static inline uint8_t LSR8( uint8_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t carry = 0x00;

    carry = ( O1 & 0x01 ) ? CARRY_FLAG : carry;
    O1 =  ( O1 >> 1 ) & 0x7F;
    zero = ( O1 == 0x00 ) ? ZERO_FLAG : 0x00;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | CARRY_FLAG ) )
        | zero | carry;

    return O1;
}

static inline uint16_t LSR16( uint16_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t carry = 0x00;

    uint16_t value = O1;
    carry = ( value & 0x0001 ) ? CARRY_FLAG : carry;
    value =  ( value >> 1 ) & 0x7FFF;
    zero = ( value == 0x00 ) ? ZERO_FLAG : 0x00;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | CARRY_FLAG ) )
        | zero | carry;

    return value;
}

static inline void LSRMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, LSR8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, LSR16( MainBusReadU16( address ) ) );
    }
}

////LSR dp    46    Direct Page    N��ZC    2
void f46_LSR(){
    LSRMem( direct( 0 ) );
}

////LSR A    4A    Accumulator    N��ZC    1
void f4A_LSR(){
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | LSR8( (uint8_t)accumulator );
    }
    else {
        accumulator = LSR16( accumulator );
    }
}

////LSR addr    4E    Absolute    N��ZC    3
void f4E_LSR(){
    LSRMem( absolute( 0 ) );
}

////LSR dp, X    56    DP Indexed, X    N��ZC    2
void f56_LSR(){
    LSRMem( direct( X ) );
}

////LSR addr, X    5E    Absolute Indexed, X    N��ZC    3
void f5E_LSR(){
    LSRMem( absoluteIndexedX() );
}
#pragma endregion

////MVN srcbk, destbk    54    Block Move        3
void f54_MVN(){
    // TODO - bank boundaries? ANS - bank shouldn't change, offset wraps
    // TODO - mapping boundaries?
    // TODO - m/x flags?
    // TODO - might be better to loop instead of memcpy

    uint8_t destBank = immediate();
    uint8_t srcBank = immediate();
    DBR = destBank;

    MemoryAddress src = GetBusAddress( srcBank, X );
    MemoryAddress dest = GetBusAddress( destBank, Y );
    MainBusWriteU8( dest, MainBusReadU8( src ) );
    ++X;
    ++Y;
    --accumulator;
    if ( accumulator != 0xFFFF ) {
        nextOperationOffset = currentOperationOffset;
    }
}

////MVP srcbk, destbk    44    Block Move        3
void f44_MVP(){
    // TODO - bank boundaries? ANS - bank shouldn't change, offset wraps
    // TODO - mapping boundaries?
    // TODO - m/x flags?
    // TODO - might be better to loop instead of memcpy

    uint8_t destBank = immediate();
    uint8_t srcBank = immediate();
    DBR = destBank;

    MemoryAddress src = GetBusAddress( srcBank, X );
    MemoryAddress dest = GetBusAddress( destBank, Y );
    MainBusWriteU8( dest, MainBusReadU8( src ) );

    --X;
    --Y;
    --accumulator;
    if ( accumulator != 0xFFFF ) {
        nextOperationOffset = currentOperationOffset;
    }
}

////NOP    EA    Implied        1
void fEA_NOP(){
}

#pragma region ORA

static inline void ORA8( uint8_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t negative = 0x00;

    accumulator = ( accumulator & 0xFF00 ) | ( ( accumulator & 0x00FF ) | O1 );
    zero = ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : zero;
    negative = ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : zero;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | zero | negative;
}

static inline void ORA16( uint16_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t negative = 0x00;

    accumulator |= O1;
    zero = ( accumulator == 0 ) ? ZERO_FLAG : zero;
    negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : zero;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | zero | negative;
}

static inline void ORAMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        ORA8( MainBusReadU8( address ) );
    }
    else {
        ORA16( MainBusReadU16( address ) );
    }
}

////ORA(_dp, _X)    1    DP Indexed Indirect, X    N��Z - 2
void f01_ORA(){
    ORAMem( directIndexedXIndirect() );
}

////ORA sr, S    3    Stack Relative    N��Z - 2
void f03_ORA(){
    ORAMem( stackRelative() );
}

////ORA dp    5    Direct Page    N��Z - 2
void f05_ORA(){
    ORAMem( direct( 0 ) );
}

////ORA[_dp_]    7    DP Indirect Long    N��Z - 2
void f07_ORA(){
    ORAMem( directIndirectLong() );
}

////ORA #const    9    Immediate    N��Z - 2
void f09_ORA(){
    if ( !( p_register & M_FLAG ) ) {
        ORA8( immediate() );
    }
    else {
        ORA16( immediateU16() );
        ++nextOperationOffset;
    }
}

////ORA addr    0D    Absolute    N��Z - 3
void f0D_ORA(){
    ORAMem( absolute( 0 ) );
}

////ORA long    0F    Absolute Long    N��Z - 4
void f0F_ORA(){
    ORAMem( absoluteLong( 0 ) );
}

////ORA(_dp_), Y    11    DP Indirect Indexed, Y    N��Z - 2
void f11_ORA(){
    ORAMem( directIndirectIndexedY( 0 ) );
}

////ORA(_dp_)    12    DP Indirect    N��Z - 2
void f12_ORA(){
    ORAMem( directIndirect() );
}

////ORA(_sr_, S), Y    13    SR Indirect Indexed, Y    N��Z - 2
void f13_ORA(){
    ORAMem( stackRelativeIndirectIndexedY() );
}

////ORA dp, X    15    DP Indexed, X    N��Z - 2
void f15_ORA(){
    ORAMem( direct( X ) );
}

////ORA[_dp_], Y    17    DP Indirect Long Indexed, Y    N��Z - 2
void f17_ORA(){
    ORAMem( directIndirectIndexedYLong() );
}

////ORA addr, Y    19    Absolute Indexed, Y    N��Z - 3
void f19_ORA(){
    ORAMem( absoluteIndexedY() );
}

////ORA addr, X    1D    Absolute Indexed, X    N��Z - 3
void f1D_ORA(){
    ORAMem( absoluteIndexedX() );
}

////ORA long, X    1F    Absolute Long Indexed, X    N��Z - 4
void f1F_ORA(){
    ORAMem( absoluteLongIndexedX() );
}
#pragma endregion

#pragma region stack

////PEA addr    F4    Stack(Absolute)        3
void fF4_PEA(){
    pushU16( immediateU16() );
}

////PEI(dp)    D4    Stack(DP Indirect)        2
void fD4_PEI(){
    pushU16( MainBusReadU16( direct( 0 ) ) );
}

////PER label    62    Stack(PC Relative Long)        3
void f62_PER(){
    // TODO - verify signed/unsigned appropriateness
    pushU16( nextOperationOffset + (int16_t)immediateU16() );
}

////PHA    48    Stack(Push)        1
void f48_PHA(){
    if( p_register & M_FLAG ) {
        pushU8( (uint8_t)( accumulator & 0x00FF ) );
    }
    else {
        pushU16( accumulator );
    }
}

////PHB    8B    Stack(Push)        1
void f8B_PHB(){
    pushU8( DBR );
}

////PHD    0B    Stack(Push)        1
void f0B_PHD(){
    pushU16( DP );
}

////PHK    4B    Stack(Push)        1
void f4B_PHK(){
    pushU8( PBR );
}

////PHP    8    Stack(Push)        1
void f08_PHP(){
    pushU16( p_register );
}

////PHX    DA    Stack(Push)        1
void fDA_PHX(){
    if ( p_register & X_FLAG ) {
        pushU8( (uint8_t) ( X & 0x00FF ) );
    }
    else {
        pushU16( X );
    }
}

////PHY    5A    Stack(Push)        1
void f5A_PHY(){
    if ( p_register & X_FLAG ) {
        pushU8( (uint8_t) ( Y & 0x00FF ) );
    }
    else {
        pushU16( Y );
    }
}

static void PL( uint8_t *target, bool halfWord ) {
    uint16_t zero = 0x00;
    uint16_t negative = 0x00;

    if ( halfWord ) {
        *target = popU8();
        zero = ( *target == 0 ) ? ZERO_FLAG : zero;
        negative = ( *target & 0x80 ) ? NEGATIVE_FLAG : negative;
    }
    else {
        uint16_t value = popU16();
        storeU16( target, value );
        zero = ( value == 0 ) ? ZERO_FLAG : zero;
        negative = ( value & 0x8000 ) ? NEGATIVE_FLAG : negative;
    }
    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG ) )
        | zero | negative;
}

////PLA    68    Stack(Pull)    N��Z - 1
void f68_PLA(){
    PL( (uint8_t*) &accumulator, p_register & M_FLAG );
}

////PLB    AB    Stack(Pull)    N��Z - 1
void fAB_PLB(){
    PL( &DBR, true );
}

////PLD    2B    Stack(Pull)    N��Z - 1
void f2B_PLD(){
    PL( (uint8_t*) &DP, false );
}

////PLP    28    Stack(Pull)    N��Z - 1
void f28_PLP(){
    PL( &p_register, true );
}

////PLX    FA    Stack(Pull)    N��Z - 1
void fFA_PLX(){
    PL( (uint8_t*) &X, p_register & X_FLAG );
}

////PLY    7A    Stack(Pull)    N��Z - 1
void f7A_PLY(){
    PL( (uint8_t*) &Y, p_register & X_FLAG );
}
#pragma endregion

#pragma region rot

static inline uint8_t ROL8( uint8_t O1 ) {
    uint16_t zero = 0x00;
    uint16_t negative = 0x00;
    uint16_t carry = 0x00;

    carry = ( O1 & 0x80 ) ? CARRY_FLAG : carry;
    O1 = ( O1 << 1 ) | ( ( p_register & CARRY_FLAG ) ? 0x01 : 0x00 );
    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;

    return O1;
}
static inline uint16_t ROL16( uint16_t O1 ) {
    uint16_t zero = 0x00;
    uint16_t negative = 0x00;
    uint16_t carry = 0x00;

    uint16_t value = O1;
    carry = ( value & 0x8000 ) ? CARRY_FLAG : carry;
    value = ( value << 1 ) | ( ( p_register & CARRY_FLAG ) ? 0x01 : 0x00 );
    negative = ( value & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( value == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;

    return value;
}
static inline void ROLMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, ROL8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, ROL16( MainBusReadU16( address ) ) );
    }
}

static inline uint8_t ROR8( uint8_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t negative = 0x00;
    uint8_t carry = 0x00;

    carry = ( O1 & 0x80 ) ? CARRY_FLAG : carry;
    O1 = ( O1 >> 1 ) | ( ( p_register & CARRY_FLAG ) ? 0x80 : 0x00 );
    negative = ( O1 & 0x80 ) ? NEGATIVE_FLAG : negative;
    zero = ( O1 == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;

    return O1;
}
static inline uint16_t ROR16( uint16_t O1 ) {
    uint8_t zero = 0x00;
    uint8_t negative = 0x00;
    uint8_t carry = 0x00;

    uint16_t value = O1;
    carry = ( value & 0x8000 ) ? CARRY_FLAG : carry;
    value = ( value >> 1 ) | ( ( p_register & CARRY_FLAG ) ? 0x8000 : 0x00 );
    negative = ( value & 0x8000 ) ? NEGATIVE_FLAG : negative;
    zero = ( value == 0 ) ? ZERO_FLAG : zero;

    p_register = ( p_register & ~( ZERO_FLAG | NEGATIVE_FLAG | CARRY_FLAG ) )
        | zero | negative | carry;

    return value;
}
static inline void RORMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, ROR8( MainBusReadU8( address ) ) );
    }
    else {
        MainBusWriteU16( address, ROR16( MainBusReadU16( address ) ) );
    }
}


////ROL dp    26    Direct Page    N��ZC    2
void f26_ROL(){
    ROLMem( direct( 0 ) );
}

////ROL A    2A    Accumulator    N��ZC    1
void f2A_ROL(){
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | (uint16_t)ROL8( (uint8_t)accumulator  );
    }
    else {
        ROL16( accumulator  );
    }
}

////ROL addr    2E    Absolute    N��ZC    3
void f2E_ROL(){
    ROLMem( absolute( 0 ) );
}

////ROL dp, X    36    DP Indexed, X    N��ZC    2
void f36_ROL(){
    ROLMem( direct( X ) );
}

////ROL addr, X    3E    Absolute Indexed, X    N��ZC    3
void f3E_ROL(){
    ROLMem( absoluteIndexedX() );
}

////ROR dp    66    Direct Page    N��ZC    2
void f66_ROR(){
    RORMem( direct( 0 ) );
}

////ROR A    6A    Accumulator    N��ZC    1
void f6A_ROR(){
    if ( p_register & M_FLAG ) {
        ROR8( (uint8_t) accumulator  );
    }
    else {
        ROR16( accumulator  );
    }
}

////ROR addr    6E    Absolute    N��ZC    3
void f6E_ROR(){
    RORMem( absolute( 0 ) );
}

////ROR dp, X    76    DP Indexed, X    N��ZC    2
void f76_ROR(){
    RORMem( direct( X ) );
}

////ROR addr, X    7E    Absolute Indexed, X    N��ZC    3
void f7E_ROR(){
    RORMem( absoluteIndexedX() );
}

#pragma endregion

#pragma region returns
// TODO - might need to increment PC here?

////RTI    40    Stack(RTI)    NVMXDIZC    1
void f40_RTI(){
    // TODO - P reg or PC first?
    // TODO - M/X flags might not be affected in E mode
    p_register = popU8();
    nextOperationOffset = popU16();
    if ( !inEmulationMode ) {
        PBR = popU8();
    }
    inIRQHandler = false;
}

////RTL    6B    Stack(RTL)        1
void f6B_RTL(){
    nextOperationOffset = popU16() + 1;
    PBR = popU8();
}

////RTS    60    Stack(RTS)        1
void f60_RTS(){
    nextOperationOffset = popU16() + 1;
}

void executeIRQ( Vectors vector ) {
    uint8_t pRegisterToPush = p_register;
    if ( !inEmulationMode ) {
        pushU8( PBR );
    }
    else if ( vector == Vector_BRK ) {
        p_register |= M_FLAG; // M_FLAG is B_FLAG in emulation mode (TODO - add dedicated B_FLAG)
    }
    PBR = 0x00;
    pushU16( nextOperationOffset );
    pushU8( pRegisterToPush );

    nextOperationOffset = GetVectorValue( vector );
    inIRQHandler = true;
}

void executeNMI() {
    if ( !inEmulationMode ) {
        pushU8( PBR );
    }
    PBR = 0x00;
    pushU16( nextOperationOffset );
    pushU8( p_register );

    nextOperationOffset = GetVectorValue( Vector_NMI );
}

#pragma endregion

#pragma region SBC
static inline void SBC8( uint8_t O1 ) {
    // TODO - Implement this properly (set carry, overflow, do BCD)
    uint8_t carry = ( p_register & CARRY_FLAG ) ? 0x00 : 0x01;
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;
    uint8_t zero = 0x00;

    uint8_t value = O1;
    uint8_t accValue = (uint8_t) ( accumulator & 0x00FF );
    uint8_t result = 0x00;

    if ( p_register & DECIMAL_FLAG ) {
        // TODO
    }
    else {
        result = accValue - value - carry;
    }
    accumulator = ( accumulator & 0xFF00 ) | (uint16_t) value;
    negative = ( value & 0x80 ) ? NEGATIVE_FLAG : 0x00;
    zero = ( value == 0x00 ) ? ZERO_FLAG : 0x00;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | OVERFLOW_FLAG ) )
        | zero | negative | overflow;
}
static inline void SBC16( uint16_t O1 ) {
    // TODO - Implement this properly (set carry, overflow, do BCD)
    uint8_t carry = ( p_register & CARRY_FLAG ) ? 0x00 : 0x01;
    uint8_t negative = 0x00;
    uint8_t overflow = 0x00;
    uint8_t zero = 0x00;

    uint16_t value = O1;
    uint16_t result = 0x00;
    if ( p_register & DECIMAL_FLAG ) {
        // TODO
    }
    else {
        result = accumulator - value - carry;
    }
    accumulator = result;
    negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : 0x00;
    zero = ( accumulator == 0x00 ) ? ZERO_FLAG : 0x00;

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG | OVERFLOW_FLAG ) )
        | zero | negative | overflow;
}
static inline void SBCMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        SBC8( MainBusReadU8( address ) );
    }
    else {
        SBC16( MainBusReadU16( address ) );
    }
}

////SBC(_dp, _X)    E1    DP Indexed Indirect, X    NV� - ZC    2
void fE1_SBC(){
    SBCMem( directIndexedXIndirect() );

}

////SBC sr, S    E3    Stack Relative    NV� - ZC    2
void fE3_SBC(){
    SBCMem( stackRelative() );
}

////SBC dp    E5    Direct Page    NV� - ZC    2
void fE5_SBC(){
    SBCMem( direct( 0 ) );
}

////SBC[_dp_]    E7    DP Indirect Long    NV� - ZC    2
void fE7_SBC(){
    SBCMem( directIndirectLong() );
}

////SBC #const    E9    Immediate    NV� - ZC    2
void fE9_SBC(){
    if ( p_register & M_FLAG ) {
        SBC8( immediate() );
    }
    else {
        SBC16( immediateU16() );
        ++nextOperationOffset;
    }
}

////SBC addr    ED    Absolute    NV� - ZC    3
void fED_SBC(){
    SBCMem( absolute( 0 ) );
}

////SBC long    EF    Absolute Long    NV� - ZC    4
void fEF_SBC(){
    SBCMem( absoluteLong( 0 ) );
}

////SBC(_dp_), Y    F1    DP Indirect Indexed, Y    NV� - ZC    2
void fF1_SBC(){
    SBCMem( directIndirectIndexedY() );
}

////SBC(_dp_)    F2    DP Indirect    NV� - ZC    2
void fF2_SBC(){
    SBCMem( directIndirect() );
}

////SBC(_sr_, S), Y    F3    SR Indirect Indexed, Y    NV� - ZC    2
void fF3_SBC(){
    SBCMem( stackRelativeIndirectIndexedY() );
}

////SBC dp, X    F5    DP Indexed, X    NV� - ZC    2
void fF5_SBC(){
    SBCMem( direct( X  ) );
}

////SBC[_dp_], Y    F7    DP Indirect Long Indexed, Y    NV� - ZC    2
void fF7_SBC(){
    SBCMem( directIndirectIndexedYLong() );
}

////SBC addr, Y    F9    Absolute Indexed, Y    NV� - ZC    3
void fF9_SBC(){
    SBCMem( absoluteIndexedY() );
}

////SBC addr, X    FD    Absolute Indexed, X    NV� - ZC    3
void fFD_SBC(){
    SBCMem( absoluteIndexedX() );
}

////SBC long, X    FF    Absolute Long Indexed, X    NV� - ZC    4
void fFF_SBC(){
    SBCMem( absoluteLongIndexedX() );
}
#pragma endregion

#pragma region p_register
////REP #const    C2    Immediate    NVMXDIZC    2
void fC2_REP() {
    p_register &= ~immediate();
}

////SEC    38    Implied    �� - C    1
void f38_SEC(){
    p_register |= CARRY_FLAG;
}

////SED    F8    Implied    � - D�    1
void fF8_SED(){
    p_register |= DECIMAL_FLAG;
}

////SEI    78    Implied    ��I�    1
void f78_SEI(){
    p_register |= INTERRUPT_FLAG;
}

////SEP    E2    Immediate    NVMXDIZC    2
void fE2_SEP(){
    uint8_t operand = immediate();
    p_register |= operand;
    if ( operand & X_FLAG ) {
        // Ground truth doesn't clear high-byte, so comment out for now
        X &= 0x00FF;
        Y &= 0x00FF;
    }
}
#pragma endregion

#pragma region store

static inline void STA( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, (uint8_t)( accumulator & 0x00FF ) );
    }
    else {
        MainBusWriteU16( address, accumulator );
    }
}


////STA(_dp, _X)    81    DP Indexed Indirect, X        2
void f81_STA(){
    STA( directIndexedXIndirect() );
}

////STA sr, S    83    Stack Relative        2
void f83_STA(){
    STA( stackRelative() );
}

////STA dp    85    Direct Page        2
void f85_STA(){
    STA( direct( 0 ) );
}

////STA[_dp_]    87    DP Indirect Long        2
void f87_STA(){
    STA( directIndirectLong() );
}

////STA addr    8D    Absolute        3
void f8D_STA(){
    STA( absolute( 0 ) );
}

////STA long    8F    Absolute Long        4
void f8F_STA(){
    STA( absoluteLong( 0 ) );
}

////STA(_dp_), Y    91    DP Indirect Indexed, Y        2
void f91_STA(){
    STA( directIndirectIndexedY( 0 ) );
}

////STA(_dp_)    92    DP Indirect        2
void f92_STA(){
    STA( directIndirect() );
}

////STA(_sr_, S), Y    93    SR Indirect Indexed, Y        2
void f93_STA(){
    STA( stackRelativeIndirectIndexedY() );
}

////STA dpX    95    DP Indexed, X        2
void f95_STA(){
    STA( direct( X ) );
}

////STA[_dp_], Y    97    DP Indirect Long Indexed, Y        2
void f97_STA(){
    STA( directIndirectIndexedYLong() );
}

////STA addr, Y    99    Absolute Indexed, Y        3
void f99_STA(){
    STA( absoluteIndexedY() );
}

////STA addr, X    9D    Absolute Indexed, X        3
void f9D_STA(){
    STA( absoluteIndexedX() );
}

////STA long, X    9F    Absolute Long Indexed, X        4
void f9F_STA(){
    STA( absoluteLongIndexedX() );
}

static inline void STN( uint16_t registerValue, MemoryAddress address ) {
    if ( p_register & X_FLAG ) {
        MainBusWriteU8( address, (uint8_t)( registerValue & 0x00FF ) );
    }
    else {
        MainBusWriteU16( address, registerValue );
    }
}

////STX dp    86    Direct Page        2
void f86_STX(){
    STN( X, direct( 0 ) );
}

////STX addr    8E    Absolute        3
void f8E_STX(){
    STN( X, absolute( 0 ) );
}

////STX dp, Y    96    DP Indexed, Y        2
void f96_STX(){
    STN( X, direct( Y ) );
}

////STY dp    84    Direct Page        2
void f84_STY(){
    STN( Y, direct( 0 ) );
}

////STY addr    8C    Absolute        3
void f8C_STY(){
    STN( Y, absolute( 0 ) );
}

////STY dp, X    94    DP Indexed, X        2
void f94_STY(){
    STN( Y, direct( X ) );
}

static inline void STZMem( MemoryAddress address ) {
    if ( p_register & M_FLAG ) {
        MainBusWriteU8( address, 0 );
    }
    else {
        MainBusWriteU16( address, 0 );
    }
}

////STZ dp    64    Direct Page        2
void f64_STZ(){
    STZMem( direct( 0 ) );
}

////STZ dp, X    74    DP Indexed, X        2
void f74_STZ(){
    STZMem( direct( X ) );
}

////STZ addr    9C    Absolute        3
void f9C_STZ(){
    STZMem( absolute( 0 ) );
}

////STZ addr, X    9E    Absolute Indexed, X        3
void f9E_STZ(){
    STZMem( absoluteIndexedX() );
}

#pragma endregion

#pragma region test_reset

static inline void TRB( MemoryAddress address ) {
    uint16_t zero = ZERO_FLAG;
    if ( p_register & M_FLAG ) {
        uint8_t value = MainBusReadU8( address );
        value &= ~( (uint8_t)( accumulator & 0x00FF ) );
        zero = ( value == 0 ) ? ZERO_FLAG : 0x00;
        MainBusWriteU8( address, value );
    }
    else {
        uint16_t value = MainBusReadU16( address );
        value &= ~accumulator;
        zero = ( value == 0 ) ? ZERO_FLAG : 0x00;
        MainBusWriteU16( address, value );
    }

    p_register = ( p_register & ~ZERO_FLAG ) | zero;
}

////TRB dp    14    Direct Page    ��Z - 2
void f14_TRB(){
    TRB( direct( 0 ) );
}

////TRB addr    1C    Absolute    ��Z - 3
void f1C_TRB(){
    TRB( absolute( 0 ) );
}

static inline void TSB( MemoryAddress address ) {
    uint16_t zero = ZERO_FLAG;
    if ( p_register & M_FLAG ) {
        uint8_t accVal = (uint8_t)( accumulator & 0x00FF );
        uint8_t value = MainBusReadU8( address );
        value |= accVal;
        zero = ( ( accVal & value ) == 0 ) ? ZERO_FLAG : 0x00;
        MainBusWriteU8( address, value );
    }
    else {
        uint16_t value = MainBusReadU16( address );
        value |= accumulator;
        zero = ( ( value & accumulator ) == 0 ) ? ZERO_FLAG : 0x00;
        MainBusWriteU16( address, value );
    }

    p_register = ( p_register & ~ZERO_FLAG ) | zero;
}

////TSB dp    4    Direct Page    ��Z - 2
void f04_TSB(){
    TSB( direct( 0 ) );
}

////TSB addr    0C    Absolute    ��Z - 3
void f0C_TSB(){
    TSB( absolute( 0 ) );
}
#pragma endregion

#pragma region transfers
static inline void TA( uint8_t *target ) {
    uint16_t negative = 0x00;
    if ( p_register & X_FLAG ) {
        *target = accumulator & 0x00FF;
        negative = ( *target & 0x0080 ) ? NEGATIVE_FLAG : negative;
    }
    else {
        storeU16( target, accumulator );
        negative = ( *target & 0x8000 ) ? NEGATIVE_FLAG : negative;
    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( *target == 0 ) ? ZERO_FLAG : 0x00 )
        | negative;
}
////TAX    AA    Implied    N��Z - 1
void fAA_TAX() {
    TA( (uint8_t*)&X );
}

////TAY    A8    Implied    N��Z - 1
void fA8_TAY() {
    TA( (uint8_t*)&Y );
}

////TCD    5B    Implied    N��Z - 1
void f5B_TCD() {
    DP = accumulator;
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( DP == 0 ) ? ZERO_FLAG : 0x00 )
        | ( ( DP & 0x8000 ) ? NEGATIVE_FLAG : 0x00 );
}

////TCS    1B    Implied        1
void f1B_TCS() {
    SP = accumulator;
    if ( inEmulationMode ) {
        SP &= 0x00FF;
        SP |= 0x0100;
    }
}

////TDC    7B    Implied    N��Z - 1
void f7B_TDC() {
    accumulator = DP;
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( accumulator == 0 ) ? ZERO_FLAG : 0x00 )
        | ( ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : 0x00 );
}

////TSC    3B    Implied    N��Z - 1
void f3B_TSC(){
    accumulator = SP;
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( accumulator == 0 ) ? ZERO_FLAG : 0x00 )
        | ( ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : 0x00 );
}

////TSX    BA    Implied    N��Z - 1
void fBA_TSX(){
    X = SP;
    uint16_t negative = 0x00;
    if ( p_register & X_FLAG ) {
        X &= 0x00FF;
        negative = ( X & 0x80 ) ? NEGATIVE_FLAG : negative;
    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( X == 0 ) ? ZERO_FLAG : 0x00 )
        | negative;
}

////TXA    8A    Implied    N��Z - 1
void f8A_TXA(){
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | ( X & 0x00FF );
        negative = ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : negative;
        zero = ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : zero;
    }
    else {
        accumulator = X;
        if ( p_register & X_FLAG ) {
            accumulator &= 0x00FF;
        }
        negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : negative;
        zero = ( accumulator == 0 ) ? ZERO_FLAG : zero;

    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | negative | zero;
}

////TXS    9A    Implied        1
void f9A_TXS(){
    if ( inEmulationMode || ( p_register & X_FLAG ) ) {
        SP = X & 0x00FF;
    }
    else {
        SP = X;
    }
}

////TXY    9B    Implied    N��Z - 1
void f9B_TXY(){
    Y = X;
    uint16_t nMask = ( p_register & X_FLAG ) ? 0x0080 : 0x8000;
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( Y & nMask ) ? NEGATIVE_FLAG : 0x00 )
        | ( ( Y == 0 ) ? ZERO_FLAG : 0x00 );
}

////TYA    98    Implied    N��Z - 1
void f98_TYA(){
    uint16_t negative = 0x00;
    uint16_t zero = 0x00;
    if ( p_register & M_FLAG ) {
        accumulator = ( accumulator & 0xFF00 ) | ( Y & 0x00FF );
        negative = ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : negative;
        zero = ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : zero;
    }
    else {
        accumulator = Y;
        if ( p_register & X_FLAG ) {
            accumulator &= 0x00FF;
        }
        negative = ( accumulator & 0x8000 ) ? NEGATIVE_FLAG : negative;
        zero = ( accumulator == 0 ) ? ZERO_FLAG : zero;

    }
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | negative | zero;
}

////TYX    BB    Implied    N��Z - 1
void fBB_TYX(){
    X = Y;
    uint16_t nMask = ( p_register & X_FLAG ) ? 0x0080 : 0x8000;
    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( X & nMask ) ? NEGATIVE_FLAG : 0x00 )
        | ( ( X == 0 ) ? ZERO_FLAG : 0x00 );
}
#pragma endregion

////WAI    CB    Implied        1
void fCB_WAI(){
    // TODO
    nextOperationOffset = currentOperationOffset;
}

//STP    DB    Implied        1
void fDB_STP() {
    // TODO
}

////WDM    42            2
void f42_WDM(){
    // TODO
}

//XBA    EB    Implied    N��Z - 1
void fEB_XBA(){
    accumulator = ( accumulator >> 8 ) | ( accumulator << 8 );

    p_register = ( p_register & ~( NEGATIVE_FLAG | ZERO_FLAG ) )
        | ( ( accumulator & 0x0080 ) ? NEGATIVE_FLAG : 0x00 )
        | ( ( ( accumulator & 0x00FF ) == 0 ) ? ZERO_FLAG : 0x00 );
}

//XCE    FB    Implied    �MX�CE    1
// Exchange carry and emulation flags
void fFB_XCE(){
    uint8_t carryVal = p_register & CARRY_FLAG;

    p_register &= ~CARRY_FLAG;

    // TODO - Ground truth is apparently broken, so nix this while developing
    if ( inEmulationMode )
        p_register |= CARRY_FLAG;

    inEmulationMode = carryVal;

    PC++;
}

static InstructionEntry instructions[ 0x100 ] = {
    { f00_BRK, 2, 7 }, // Break ; Stack/Interrupt ; ----DI--
    { f01_ORA, 2, 6 }, // (dp,X) ; OR Accumulator with Memory ; DP Indexed Indirect,X	N-----Z-
    { f02_COP, 2, 7 }, // const ; Co-Processor Enable ; Stack/Interrupt	----DI--
    { f03_ORA, 2, 4 }, // sr,S ; OR Accumulator with Memory ; Stack Relative	N-----Z-
    { f04_TSB, 2, 5 }, // dp ; Test and Set Memory Bits Against Accumulator ; Direct Page	------Z-
    { f05_ORA, 2, 3 }, // dp ; OR Accumulator with Memory ; Direct Page	N-----Z-
    { f06_ASL, 2, 5 }, // dp ; Arithmetic Shift Left ; Direct Page	N-----ZC
    { f07_ORA, 2, 6 }, // [dp] ; OR Accumulator with Memory ; DP Indirect Long	N-----Z-
    { f08_PHP, 1, 3 }, // Push Processor Status Register ; Stack (Push)
    { f09_ORA, 2, 2 }, // #const ; OR Accumulator with Memory ; Immediate	N-----Z-
    { f0A_ASL, 1, 2 }, // A ; Arithmetic Shift Left ; Accumulator	N-----ZC
    { f0B_PHD, 1, 4 }, // Push Direct Page Register ; Stack (Push)
    { f0C_TSB, 3, 6 }, // addr ; Test and Set Memory Bits Against Accumulator ; Absolute	------Z-
    { f0D_ORA, 3, 4 }, // addr ; OR Accumulator with Memory ; Absolute	N-----Z-
    { f0E_ASL, 3, 6 }, // addr ; Arithmetic Shift Left ; Absolute	N-----ZC
    { f0F_ORA, 4, 5 }, // long ; OR Accumulator with Memory ; Absolute Long	N-----Z-
    { f10_BPL, 2, 2 }, // nearlabel ; Branch if Plus ; Program Counter Relative
    { f11_ORA, 2, 5 }, // (dp),Y ; OR Accumulator with Memory ; DP Indirect Indexed, Y	N-----Z-
    { f12_ORA, 2, 5 }, // (dp) ; OR Accumulator with Memory ; DP Indirect	N-----Z-
    { f13_ORA, 2, 7 }, // (sr,S),Y ; OR Accumulator with Memory ; SR Indirect Indexed,Y	N-----Z-
    { f14_TRB, 2, 5 }, // dp ; Test and Reset Memory Bits Against Accumulator ; Direct Page	------Z-
    { f15_ORA, 2, 4 }, // dp,X ; OR Accumulator with Memory ; DP Indexed,X	N-----Z-
    { f16_ASL, 2, 6 }, // dp,X ; Arithmetic Shift Left ; DP Indexed,X	N-----ZC
    { f17_ORA, 2, 6 }, // [dp],Y ; OR Accumulator with Memory ; DP Indirect Long Indexed, Y	N-----Z-
    { f18_CLC, 1, 2 }, // Clear Carry ; Implied ; -------C
    { f19_ORA, 3, 4 }, // addr,Y ; OR Accumulator with Memory ; Absolute Indexed,Y	N-----Z-
    { f1A_INC, 1, 2 }, // A	INA ; Increment ; Accumulator	N-----Z-
    { f1B_TCS, 1, 2 }, // Transfer 16-bit Accumulator to Stack Pointer ; Implied
    { f1C_TRB, 3, 6 }, // addr ; Test and Reset Memory Bits Against Accumulator ; Absolute	------Z-
    { f1D_ORA, 3, 4 }, // addr,X ; OR Accumulator with Memory ; Absolute Indexed,X	N-----Z-
    { f1E_ASL, 3, 7 }, // addr,X ; Arithmetic Shift Left ; Absolute Indexed,X	N-----ZC
    { f1F_ORA, 4, 5 }, // long,X ; OR Accumulator with Memory ; Absolute Long Indexed,X	N-----Z-
    { f20_JSR, 3, 6 }, // addr ; Jump to Subroutine ; Absolute
    { f21_AND, 2, 6 }, // (dp,X) ; AND Accumulator with Memory ; DP Indexed Indirect,X	N-----Z-
    { f22_JSR, 4, 8 }, // long	JSL ; Jump to Subroutine ; Absolute Long
    { f23_AND, 2, 4 }, // sr,S ; AND Accumulator with Memory ; Stack Relative	N-----Z-
    { f24_BIT, 2, 3 }, // dp ; Test Bits ; Direct Page	NV----Z-
    { f25_AND, 2, 3 }, // dp ; AND Accumulator with Memory ; Direct Page	N-----Z-
    { f26_ROL, 2, 5 }, // dp ; Rotate Memory or Accumulator Left ; Direct Page	N-----ZC
    { f27_AND, 2, 6 }, // [dp] ; AND Accumulator with Memory ; DP Indirect Long	N-----Z-
    { f28_PLP, 1, 4 }, // Pull Processor Status Register ; Stack (Pull) ; NVMXDIZC
    { f29_AND, 2, 2 }, // #const ; AND Accumulator with Memory ; Immediate	N-----Z-
    { f2A_ROL, 1, 2 }, // A ; Rotate Memory or Accumulator Left ; Accumulator	N-----ZC
    { f2B_PLD, 1, 5 }, // Pull Direct Page Register ; Stack (Pull) ; N-----Z-
    { f2C_BIT, 3, 4 }, // addr ; Test Bits ; Absolute	NV----Z-
    { f2D_AND, 3, 4 }, // addr ; AND Accumulator with Memory ; Absolute	N-----Z-
    { f2E_ROL, 3, 6 }, // addr ; Rotate Memory or Accumulator Left ; Absolute	N-----ZC
    { f2F_AND, 4, 5 }, // long ; AND Accumulator with Memory ; Absolute Long	N-----Z-
    { f30_BMI, 2, 2 }, // nearlabel ; Branch if Minus ; Program Counter Relative
    { f31_AND, 2, 5 }, // (dp),Y ; AND Accumulator with Memory ; DP Indirect Indexed, Y	N-----Z-
    { f32_AND, 2, 5 }, // (dp) ; AND Accumulator with Memory ; DP Indirect	N-----Z-
    { f33_AND, 2, 7 }, // (sr,S),Y ; AND Accumulator with Memory ; SR Indirect Indexed,Y	N-----Z-
    { f34_BIT, 2, 4 }, // dp,X ; Test Bits ; DP Indexed,X	NV----Z-
    { f35_AND, 2, 4 }, // dp,X ; AND Accumulator with Memory ; DP Indexed,X	N-----Z-
    { f36_ROL, 2, 6 }, // dp,X ; Rotate Memory or Accumulator Left ; DP Indexed,X	N-----ZC
    { f37_AND, 2, 6 }, // [dp],Y ; AND Accumulator with Memory ; DP Indirect Long Indexed, Y	N-----Z-
    { f38_SEC, 1, 2 }, // Set Carry Flag ; Implied ; -------C
    { f39_AND, 3, 4 }, // addr,Y ; AND Accumulator with Memory ; Absolute Indexed,Y	N-----Z-
    { f3A_DEC, 1, 2 }, // A	DEA ; Decrement ; Accumulator	N-----Z-
    { f3B_TSC, 1, 2 }, // Transfer Stack Pointer to 16-bit Accumulator ; Implied ; N-----Z-
    { f3C_BIT, 3, 4 }, // addr,X ; Test Bits ; Absolute Indexed,X	NV----Z-
    { f3D_AND, 3, 4 }, // addr,X ; AND Accumulator with Memory ; Absolute Indexed,X	N-----Z-
    { f3E_ROL, 3, 7 }, // addr,X ; Rotate Memory or Accumulator Left ; Absolute Indexed,X	N-----ZC
    { f3F_AND, 4, 5 }, // long,X ; AND Accumulator with Memory ; Absolute Long Indexed,X	N-----Z-
    { f40_RTI, 1, 6 }, // Return from Interrupt ; Stack (RTI) ; NVMXDIZC
    { f41_EOR, 2, 6 }, // (dp,X) ; Exclusive-OR Accumulator with Memory ; DP Indexed Indirect,X	N-----Z-
    { f42_WDM, 2, 0 }, //	Reserved for Future Expansion	42	2	0
    { f43_EOR, 2, 4 }, // sr,S ; Exclusive-OR Accumulator with Memory ; Stack Relative	N-----Z-
    { f44_MVP, 3, 1 }, // srcbk,destbk ; Block Move Positive ; Block Move
    { f45_EOR, 2, 3 }, // dp ; Exclusive-OR Accumulator with Memory ; Direct Page	N-----Z-
    { f46_LSR, 2, 5 }, // dp ; Logical Shift Memory or Accumulator Right ; Direct Page	N-----ZC
    { f47_EOR, 2, 6 }, // [dp] ; Exclusive-OR Accumulator with Memory ; DP Indirect Long	N-----Z-
    { f48_PHA, 1, 3 }, // Push Accumulator ; Stack (Push)
    { f49_EOR, 2, 2 }, // #const ; Exclusive-OR Accumulator with Memory ; Immediate	N-----Z-
    { f4A_LSR, 1, 2 }, // A ; Logical Shift Memory or Accumulator Right ; Accumulator	N-----ZC
    { f4B_PHK, 1, 3 }, // Push Program Bank Register ; Stack (Push)
    { f4C_JMP, 3, 3 }, // addr ; Jump ; Absolute
    { f4D_EOR, 3, 4 }, // addr ; Exclusive-OR Accumulator with Memory ; Absolute	N-----Z-
    { f4E_LSR, 3, 6 }, // addr ; Logical Shift Memory or Accumulator Right ; Absolute	N-----ZC
    { f4F_EOR, 4, 5 }, // long ; Exclusive-OR Accumulator with Memory ; Absolute Long	N-----Z-
    { f50_BVC, 2, 2 }, // nearlabel ; Branch if Overflow Clear ; Program Counter Relative
    { f51_EOR, 2, 5 }, // (dp),Y ; Exclusive-OR Accumulator with Memory ; DP Indirect Indexed, Y	N-----Z-
    { f52_EOR, 2, 5 }, // (dp) ; Exclusive-OR Accumulator with Memory ; DP Indirect	N-----Z-
    { f53_EOR, 2, 7 }, // (sr,S),Y ; Exclusive-OR Accumulator with Memory ; SR Indirect Indexed,Y	N-----Z-
    { f54_MVN, 3, 1 }, // srcbk,destbk ; Block Move Negative ; Block Move
    { f55_EOR, 2, 4 }, // dp,X ; Exclusive-OR Accumulator with Memory ; DP Indexed,X	N-----Z-
    { f56_LSR, 2, 6 }, // dp,X ; Logical Shift Memory or Accumulator Right ; DP Indexed,X	N-----ZC
    { f57_EOR, 2, 6 }, // [dp],Y ; Exclusive-OR Accumulator with Memory ; DP Indirect Long Indexed, Y	N-----Z-
    { f58_CLI, 1, 2 }, // Clear Interrupt Disable Flag ; Implied ; -----I--
    { f59_EOR, 3, 4 }, // addr,Y ; Exclusive-OR Accumulator with Memory ; Absolute Indexed,Y	N-----Z-
    { f5A_PHY, 1, 3 }, // Push Index Register Y ; Stack (Push)
    { f5B_TCD, 1, 2 }, // Transfer 16-bit Accumulator to Direct Page Register ; Implied ; N-----Z-
    { f5C_JMP, 4, 4 }, // long	JML ; Jump ; Absolute Long
    { f5D_EOR, 3, 4 }, // addr,X ; Exclusive-OR Accumulator with Memory ; Absolute Indexed,X	N-----Z-
    { f5E_LSR, 3, 7 }, // addr,X ; Logical Shift Memory or Accumulator Right ; Absolute Indexed,X	N-----ZC
    { f5F_EOR, 4, 5 }, // long,X ; Exclusive-OR Accumulator with Memory ; Absolute Long Indexed,X	N-----Z-
    { f60_RTS, 1, 6 }, // Return from Subroutine ; Stack (RTS)
    { f61_ADC, 2, 6 }, // (dp,X) ; Add With Carry ; DP Indexed Indirect,X	NV----ZC
    { f62_PER, 3, 6 }, // label ; Push Effective PC Relative Indirect Address ; Stack (PC Relative Long)
    { f63_ADC, 2, 4 }, // sr,S ; Add With Carry ; Stack Relative	NV----ZC
    { f64_STZ, 2, 3 }, // dp ; Store Zero to Memory ; Direct Page
    { f65_ADC, 2, 3 }, // dp ; Add With Carry ; Direct Page	NV----ZC
    { f66_ROR, 2, 5 }, // dp ; Rotate Memory or Accumulator Right ; Direct Page	N-----ZC
    { f67_ADC, 2, 6 }, // [dp] ; Add With Carry ; DP Indirect Long	NV----ZC
    { f68_PLA, 1, 4 }, // Pull Accumulator ; Stack (Pull) ; N-----Z-
    { f69_ADC, 2, 2 }, // #const ; Add With Carry ; Immediate	NV----ZC
    { f6A_ROR, 1, 2 }, // A ; Rotate Memory or Accumulator Right ; Accumulator	N-----ZC
    { f6B_RTL, 1, 6 }, // Return from Subroutine Long ; Stack (RTL)
    { f6C_JMP, 3, 5 }, // (addr) ; Jump ; Absolute Indirect
    { f6D_ADC, 3, 4 }, // addr ; Add With Carry ; Absolute	NV----ZC
    { f6E_ROR, 3, 6 }, // addr ; Rotate Memory or Accumulator Right ; Absolute	N-----ZC
    { f6F_ADC, 4, 5 }, // long ; Add With Carry ; Absolute Long	NV----ZC
    { f70_BVS, 2, 2 }, // nearlabel ; Branch if Overflow Set ; Program Counter Relative
    { f71_ADC, 2, 5 }, // ( dp),Y ; Add With Carry ; DP Indirect Indexed, Y	NV----ZC
    { f72_ADC, 2, 5 }, // (dp) ; Add With Carry ; DP Indirect	NV----ZC
    { f73_ADC, 2, 7 }, // (sr,S),Y ; Add With Carry ; SR Indirect Indexed,Y	NV----ZC
    { f74_STZ, 2, 4 }, // dp,X ; Store Zero to Memory ; DP Indexed,X
    { f75_ADC, 2, 4 }, // dp,X ; Add With Carry ; DP Indexed,X	NV----ZC
    { f76_ROR, 2, 6 }, // dp,X ; Rotate Memory or Accumulator Right ; DP Indexed,X	N-----ZC
    { f77_ADC, 2, 6 }, // [dp],Y ; Add With Carry ; DP Indirect Long Indexed, Y	NV----ZC
    { f78_SEI, 1, 2 }, // Set Interrupt Disable Flag ; Implied ; -----I--
    { f79_ADC, 3, 4 }, // addr,Y ; Add With Carry ; Absolute Indexed,Y	NV----ZC
    { f7A_PLY, 1, 4 }, // Pull Index Register Y ; Stack (Pull) ; N-----Z-
    { f7B_TDC, 1, 2 }, // Transfer Direct Page Register to 16-bit Accumulator ; Implied ; N-----Z-
    { f7C_JMP, 3, 6 }, // (addr,X) ; Jump ; Absolute Indexed Indirect
    { f7D_ADC, 3, 4 }, // addr,X ; Add With Carry ; Absolute Indexed,X	NV----ZC
    { f7E_ROR, 3, 7 }, // addr,X ; Rotate Memory or Accumulator Right ; Absolute Indexed,X	N-----ZC
    { f7F_ADC, 4, 5 }, // long,X ; Add With Carry ; Absolute Long Indexed,X	NV----ZC
    { f80_BRA, 2, 3 }, // nearlabel ; Branch Always ; Program Counter Relative
    { f81_STA, 2, 6 }, // (dp,X) ; Store Accumulator to Memory ; DP Indexed Indirect,X
    { f82_BRL, 3, 4 }, // label ; Branch Long Always ; Program Counter Relative Long
    { f83_STA, 2, 4 }, // sr,S ; Store Accumulator to Memory ; Stack Relative
    { f84_STY, 2, 3 }, // dp ; Store Index Register Y to Memory ; Direct Page
    { f85_STA, 2, 3 }, // dp ; Store Accumulator to Memory ; Direct Page
    { f86_STX, 2, 3 }, // dp ; Store Index Register X to Memory ; Direct Page
    { f87_STA, 2, 6 }, // [dp] ; Store Accumulator to Memory ; DP Indirect Long
    { f88_DEY, 1, 2 }, // Decrement Index Register Y ; Implied ; N-----Z-
    { f89_BIT, 2, 2 }, // #const ; Test Bits ; Immediate	------Z-
    { f8A_TXA, 1, 2 }, // Transfer Index Register X to Accumulator ; Implied ; N-----Z-
    { f8B_PHB, 1, 3 }, // Push Data Bank Register ; Stack (Push)
    { f8C_STY, 3, 4 }, // addr ; Store Index Register Y to Memory ; Absolute
    { f8D_STA, 3, 4 }, // addr ; Store Accumulator to Memory ; Absolute
    { f8E_STX, 3, 4 }, // addr ; Store Index Register X to Memory ; Absolute
    { f8F_STA, 4, 5 }, // long ; Store Accumulator to Memory ; Absolute Long
    { f90_BCC, 2, 2 }, // nearlabel	BLT ; Branch if Carry Clear ; Program Counter Relative
    { f91_STA, 2, 6 }, // (dp),Y ; Store Accumulator to Memory ; DP Indirect Indexed, Y
    { f92_STA, 2, 5 }, // (dp) ; Store Accumulator to Memory ; DP Indirect
    { f93_STA, 2, 7 }, // (sr,S),Y ; Store Accumulator to Memory ; SR Indirect Indexed,Y
    { f94_STY, 2, 4 }, // dp,X ; Store Index Register Y to Memory ; DP Indexed,X
    { f95_STA, 2, 4 }, // _dp_X ; Store Accumulator to Memory ; DP Indexed,X
    { f96_STX, 2, 4 }, // dp,Y ; Store Index Register X to Memory ; DP Indexed,Y
    { f97_STA, 2, 6 }, // [dp],Y ; Store Accumulator to Memory ; DP Indirect Long Indexed, Y
    { f98_TYA, 1, 2 }, // Transfer Index Register Y to Accumulator ; Implied ; N-----Z-
    { f99_STA, 3, 5 }, // addr,Y ; Store Accumulator to Memory ; Absolute Indexed,Y
    { f9A_TXS, 1, 2 }, // Transfer Index Register X to Stack Pointer ; Implied
    { f9B_TXY, 1, 2 }, // Transfer Index Register X to Index Register Y ; Implied ; N-----Z-
    { f9C_STZ, 3, 4 }, // addr ; Store Zero to Memory ; Absolute
    { f9D_STA, 3, 5 }, // addr,X ; Store Accumulator to Memory ; Absolute Indexed,X
    { f9E_STZ, 3, 5 }, // addr,X ; Store Zero to Memory ; Absolute Indexed,X
    { f9F_STA, 4, 5 }, // long,X ; Store Accumulator to Memory ; Absolute Long Indexed,X
    { fA0_LDY, 2, 2 }, // #const ; Load Index Register Y from Memory ; Immediate	N-----Z-
    { fA1_LDA, 2, 6 }, // (dp,X) ; Load Accumulator from Memory ; DP Indexed Indirect,X	N-----Z-
    { fA2_LDX, 2, 2 }, // #const ; Load Index Register X from Memory ; Immediate	N-----Z-
    { fA3_LDA, 2, 4 }, // sr,S ; Load Accumulator from Memory ; Stack Relative	N-----Z-
    { fA4_LDY, 2, 3 }, // dp ; Load Index Register Y from Memory ; Direct Page	N-----Z-
    { fA5_LDA, 2, 3 }, // dp ; Load Accumulator from Memory ; Direct Page	N-----Z-
    { fA6_LDX, 2, 3 }, // dp ; Load Index Register X from Memory ; Direct Page	N-----Z-
    { fA7_LDA, 2, 6 }, // [dp] ; Load Accumulator from Memory ; DP Indirect Long	N-----Z-
    { fA8_TAY, 1, 2 }, // Transfer Accumulator to Index Register Y ; Implied ; N-----Z-
    { fA9_LDA, 2, 2 }, // #const ; Load Accumulator from Memory ; Immediate	N-----Z-
    { fAA_TAX, 1, 2 }, // Transfer Accumulator to Index Register X ; Implied ; N-----Z-
    { fAB_PLB, 1, 4 }, // Pull Data Bank Register ; Stack (Pull) ; N-----Z-
    { fAC_LDY, 3, 4 }, // addr ; Load Index Register Y from Memory ; Absolute	N-----Z-
    { fAD_LDA, 3, 4 }, // addr ; Load Accumulator from Memory ; Absolute	N-----Z-
    { fAE_LDX, 3, 4 }, // addr ; Load Index Register X from Memory ; Absolute	N-----Z-
    { fAF_LDA, 4, 5 }, // long ; Load Accumulator from Memory ; Absolute Long	N-----Z-
    { fB0_BCS, 2, 2 }, // nearlabel	BGE ; Branch if Carry Set ; Program Counter Relative
    { fB1_LDA, 2, 5 }, // (dp),Y ; Load Accumulator from Memory ; DP Indirect Indexed, Y	N-----Z-
    { fB2_LDA, 2, 5 }, // (dp) ; Load Accumulator from Memory ; DP Indirect	N-----Z-
    { fB3_LDA, 2, 7 }, // (sr,S),Y ; Load Accumulator from Memory ; SR Indirect Indexed,Y	N-----Z-
    { fB4_LDY, 2, 4 }, // dp,X ; Load Index Register Y from Memory ; DP Indexed,X	N-----Z-
    { fB5_LDA, 2, 4 }, // dp,X ; Load Accumulator from Memory ; DP Indexed,X	N-----Z-
    { fB6_LDX, 2, 4 }, // dp,Y ; Load Index Register X from Memory ; DP Indexed,Y	N-----Z-
    { fB7_LDA, 2, 6 }, // [dp],Y ; Load Accumulator from Memory ; DP Indirect Long Indexed, Y	N-----Z-
    { fB8_CLV, 1, 2 }, // Clear Overflow Flag ; Implied ; -V------
    { fB9_LDA, 3, 4 }, // addr,Y ; Load Accumulator from Memory ; Absolute Indexed,Y	N-----Z-
    { fBA_TSX, 1, 2 }, // Transfer Stack Pointer to Index Register X ; Implied ; N-----Z-
    { fBB_TYX, 1, 2 }, // Transfer Index Register Y to Index Register X ; Implied ; N-----Z-
    { fBC_LDY, 3, 4 }, // addr,X ; Load Index Register Y from Memory ; Absolute Indexed,X	N-----Z-
    { fBD_LDA, 3, 4 }, // addr,X ; Load Accumulator from Memory ; Absolute Indexed,X	N-----Z-
    { fBE_LDX, 3, 4 }, // addr,Y ; Load Index Register X from Memory ; Absolute Indexed,Y	N-----Z-
    { fBF_LDA, 4, 5 }, // long,X ; Load Accumulator from Memory ; Absolute Long Indexed,X	N-----Z-
    { fC0_CPY, 2, 2 }, // #const ; Compare Index Register Y with Memory ; Immediate	N-----ZC
    { fC1_CMP, 2, 6 }, // (dp,X) ; Compare Accumulator with Memory ; DP Indexed Indirect,X	N-----ZC
    { fC2_REP, 2, 3 }, // #const ; Reset Processor Status Bits ; Immediate	NVMXDIZC
    { fC3_CMP, 2, 4 }, // sr,S ; Compare Accumulator with Memory ; Stack Relative	N-----ZC
    { fC4_CPY, 2, 3 }, // dp ; Compare Index Register Y with Memory ; Direct Page	N-----ZC
    { fC5_CMP, 2, 3 }, // dp ; Compare Accumulator with Memory ; Direct Page	N-----ZC
    { fC6_DEC, 2, 5 }, // dp ; Decrement ; Direct Page	N-----Z-
    { fC7_CMP, 2, 6 }, // [dp] ; Compare Accumulator with Memory ; DP Indirect Long	N-----ZC
    { fC8_INY, 1, 2 }, // Increment Index Register Y ; Implied ; N-----Z-
    { fC9_CMP, 2, 2 }, // #const ; Compare Accumulator with Memory ; Immediate	N-----ZC
    { fCA_DEX, 1, 2 }, // Decrement Index Register X ; Implied ; N-----Z-
    { fCB_WAI, 1, 3 }, // Wait for Interrupt ; Implied
    { fCC_CPY, 3, 4 }, // addr ; Compare Index Register Y with Memory ; Absolute	N-----ZC
    { fCD_CMP, 3, 4 }, // addr ; Compare Accumulator with Memory ; Absolute	N-----ZC
    { fCE_DEC, 3, 6 }, // addr ; Decrement ; Absolute	N-----Z-
    { fCF_CMP, 4, 5 }, // long ; Compare Accumulator with Memory ; Absolute Long	N-----ZC
    { fD0_BNE, 2, 2 }, // nearlabel ; Branch if Not Equal ; Program Counter Relative
    { fD1_CMP, 2, 5 }, // (dp),Y ; Compare Accumulator with Memory ; DP Indirect Indexed, Y	N-----ZC
    { fD2_CMP, 2, 5 }, // (dp) ; Compare Accumulator with Memory ; DP Indirect	N-----ZC
    { fD3_CMP, 2, 7 }, // (sr,S),Y ; Compare Accumulator with Memory ; SR Indirect Indexed,Y	N-----ZC
    { fD4_PEI, 2, 6 }, // (dp) ; Push Effective Indirect Address ; Stack (DP Indirect)
    { fD5_CMP, 2, 4 }, // dp,X ; Compare Accumulator with Memory ; DP Indexed,X	N-----ZC
    { fD6_DEC, 2, 6 }, // dp,X ; Decrement ; DP Indexed,X	N-----Z-
    { fD7_CMP, 2, 6 }, // [dp],Y ; Compare Accumulator with Memory ; DP Indirect Long Indexed, Y	N-----ZC
    { fD8_CLD, 1, 2 }, // Clear Decimal Mode Flag ; Implied ; ----D---
    { fD9_CMP, 3, 4 }, // addr,Y ; Compare Accumulator with Memory ; Absolute Indexed,Y	N-----ZC
    { fDA_PHX, 1, 3 }, // Push Index Register X ; Stack (Push)
    { fDB_STP, 1, 3 }, // Stop Processor ; Implied
    { fDC_JMP, 3, 6 }, // [addr]	JML ; Jump ; Absolute Indirect Long
    { fDD_CMP, 3, 4 }, // addr,X ; Compare Accumulator with Memory ; Absolute Indexed,X	N-----ZC
    { fDE_DEC, 3, 7 }, // addr,X ; Decrement ; Absolute Indexed,X	N-----Z-
    { fDF_CMP, 4, 5 }, // long,X ; Compare Accumulator with Memory ; Absolute Long Indexed,X	N-----ZC
    { fE0_CPX, 2, 2 }, // #const ; Compare Index Register X with Memory ; Immediate	N-----ZC
    { fE1_SBC, 2, 6 }, // (dp,X) ; Subtract with Borrow from Accumulator ; DP Indexed Indirect,X	NV----ZC
    { fE2_SEP, 2, 3 }, // #const ; Set Processor Status Bits ; Immediate	NVMXDIZC
    { fE3_SBC, 2, 4 }, // sr,S ; Subtract with Borrow from Accumulator ; Stack Relative	NV----ZC
    { fE4_CPX, 2, 3 }, // dp ; Compare Index Register X with Memory ; Direct Page	N-----ZC
    { fE5_SBC, 2, 3 }, // dp ; Subtract with Borrow from Accumulator ; Direct Page	NV----ZC
    { fE6_INC, 2, 5 }, // dp ; Increment ; Direct Page	N-----Z-
    { fE7_SBC, 2, 6 }, // [dp] ; Subtract with Borrow from Accumulator ; DP Indirect Long	NV----ZC
    { fE8_INX, 1, 2 }, // Increment Index Register X ; Implied ; N-----Z-
    { fE9_SBC, 2, 2 }, // #const ; Subtract with Borrow from Accumulator ; Immediate	NV----ZC
    { fEA_NOP, 1, 2 }, //	No Operation	EA	Implied	1	2
    { fEB_XBA, 1, 3 }, // Exchange B and A 8-bit Accumulators ; Implied ; N-----Z-
    { fEC_CPX, 3, 4 }, // addr ; Compare Index Register X with Memory ; Absolute	N-----ZC
    { fED_SBC, 3, 4 }, // addr ; Subtract with Borrow from Accumulator ; Absolute	NV----ZC
    { fEE_INC, 3, 6 }, // addr ; Increment ; Absolute	N-----Z-
    { fEF_SBC, 4, 5 }, // long ; Subtract with Borrow from Accumulator ; Absolute Long	NV----ZC
    { fF0_BEQ, 2, 2 }, // nearlabel ; Branch if Equal ; Program Counter Relative
    { fF1_SBC, 2, 5 }, // (dp),Y ; Subtract with Borrow from Accumulator ; DP Indirect Indexed, Y	NV----ZC
    { fF2_SBC, 2, 5 }, // (dp) ; Subtract with Borrow from Accumulator ; DP Indirect	NV----ZC
    { fF3_SBC, 2, 7 }, // (sr,S),Y ; Subtract with Borrow from Accumulator ; SR Indirect Indexed,Y	NV----ZC
    { fF4_PEA, 3, 5 }, // addr ; Push Effective Absolute Address ; Stack (Absolute)
    { fF5_SBC, 2, 4 }, // dp,X ; Subtract with Borrow from Accumulator ; DP Indexed,X	NV----ZC
    { fF6_INC, 2, 6 }, // dp,X ; Increment ; DP Indexed,X	N-----Z-
    { fF7_SBC, 2, 6 }, // [dp],Y ; Subtract with Borrow from Accumulator ; DP Indirect Long Indexed, Y	NV----ZC
    { fF8_SED, 1, 2 }, // Set Decimal Flag ; Implied ; ----D---
    { fF9_SBC, 3, 4 }, // addr,Y ; Subtract with Borrow from Accumulator ; Absolute Indexed,Y	NV----ZC
    { fFA_PLX, 1, 4 }, // Pull Index Register X ; Stack (Pull) ; N-----Z-
    { fFB_XCE, 1, 2 }, // Exchange Carry and Emulation Flags ; Implied ; --MX---CE
    { fFC_JSR, 3, 8 }, // (addr,X)) ; Jump to Subroutine ; Absolute Indexed Indirect
    { fFD_SBC, 3, 4 }, // addr,X ; Subtract with Borrow from Accumulator ; Absolute Indexed,X	NV----ZC
    { fFE_INC, 3, 7 }, // addr,X ; Increment ; Absolute Indexed,X	N-----Z-
    { fFF_SBC, 4, 5 }, // long,X ; Subtract with Borrow from Accumulator ; Absolute Long Indexed,X	NV----ZC
};

#pragma endregion