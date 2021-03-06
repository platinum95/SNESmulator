
#include "spc700.h"

#include "dsp.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "system.h"

/*

    Emulation of the SPC700 sound chip
    Aint no simple chip herey boy.

    References:
    https://wiki.superfamicom.org/spc700-reference
    https://github.com/gilligan/snesdev/blob/master/docs/spc700.txt
    http://anyplatform.net/media/guides/cpus/SPC700%20Processor%20Data.txt

*/

// TODO:
// - PC incrementing
// - JMP (1F) addressing confirmation
// - Register (mem) access
// - Clear-on-read addresses
// - Timers
// - Read-back on some ops
// - General formatting tidying
// - Optimisations
// - IPL rom
// - Other TODO notes


// Little endian

/*
Basic memory map
0x0000 - 0x00FF - direct page 0
0x00F0 - 0x00FF - memory - mapped hardware registers
0x0100 - 0x01FF - direct page 1
0x0100 - 0x01FF - potential stack memory
0xFFC0 - 0xFFFF - IPL ROM
*/

// TODO - other compilers
#define ATTR_PACKED __attribute__((__packed__))
#define UNUSED2(x) (void)(x)


/* Status register flags */
#define SPC_CARRY_FLAG  0x1
#define SPC_ZERO_FLAG 0x2
#define SPC_INTERRUPT_FLAG 0x04
#define SPC_HALF_CARRY_FLAG 0x08
#define SPC_BREAK_FLAG 0x10
#define SPC_P_FLAG 0x20
#define SPC_OVERFLOW_FLAG 0x40
#define SPC_NEGATIVE_FLAG 0x80

typedef struct SPC700InstructionEntry {
    void (*instruction)();
    uint8_t opLength;
    uint8_t opCycles;
} SPC700InstructionEntry;

SPC700InstructionEntry instructions[ 0x100 ];

typedef struct ATTR_PACKED Registers {
    uint8_t undocumented;
    uint8_t control;
    uint8_t dspAddress;
    uint8_t dspData;
    uint8_t port0;
    uint8_t port1;
    uint8_t port2;
    uint8_t port3;
    uint8_t memory0;
    uint8_t memory1;
    uint8_t timer0Target;
    uint8_t timer1Target;
    uint8_t timer2Target;
    uint8_t timer0counter;
    uint8_t timer1counter;
    uint8_t timer2counter;
} Registers;

/*
static inline uint8_t spcMemoryMapRead( uint16_t addr );
static inline uint16_t spcMemoryMapReadU16( uint16_t addr ); // TODO - readU16 may need to know if page can increment, or just offset
static inline void spcMemoryMapWrite( uint16_t addr, uint8_t value );
static inline void spcMemoryMapWriteU16( uint16_t addr, uint16_t value ); // TODO - writeU16 may need to know if page can increment, or just offset
*/

/* Memory and registers */
static uint8_t APUMemory[ 0xFFFF + 0x01 ];
static uint16_t PC, next_program_counter, curr_program_counter;
static uint8_t A, X, Y, SP, PSW;
static Registers* registers = (Registers*)( APUMemory + 0X00F0 );
static uint8_t opCycles;
static uint8_t CPUWriteComPorts[ 4 ];

static uint8_t IPL_ROM[ 64 ] = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

static uint32_t timerClockCounter = 0;
static uint8_t timersStage2[ 3 ];
static bool t0Enabled = false;
static bool t1Enabled = false;
static bool t2Enabled = false;

// TODO - overhaul with better handling
static inline void updateTimers() {
    // SPC @ 1000KHz
    // T0 and T1 @ 8KHz = 125 CPU cycles per tick
    // T2 @ 64KHz = ~16 CPU cycles per tick

/*
    if ( t0Enabled ) {
        // Zero Timer 0
        timersStage2[ 0 ] = 0;
        registers->timer0counter = 0;
    }
    if ( t1Enabled ) {
        // Zero Timer 1
        timersStage2[ 1 ] = 0;
        registers->timer1counter = 0;
    }
    if ( t2Enabled ) {
        // Zero Timer 2
        timersStage2[ 2 ] = 0;
        registers->timer2counter = 0;
    }
*/
    ++timerClockCounter;

    if ( timerClockCounter == 128 ) {
        // T0/T1/T3 tick
        if ( t0Enabled && registers->timer0Target == ++timersStage2[ 0 ] ) {
            ++registers->timer0counter;
        }
        if ( t1Enabled && registers->timer1Target == ++timersStage2[ 1 ] ) {
            ++registers->timer1counter;
        }
        if ( t2Enabled && registers->timer2Target == ++timersStage2[ 2 ] ) {
            ++registers->timer2counter;
        }
        
        timerClockCounter = 0;
    }
    else if ( t2Enabled && timerClockCounter % 16 == 0 ) {
        // T2 tick
        if ( registers->timer2Target == ++timersStage2[ 2 ] ) {
            ++registers->timer2counter;
        }
    }
}

/* Initialise (power on) */
void spc700Initialise() {
    registers->port0 = 0xAA;
    registers->port1 = 0xBB;
    CPUWriteComPorts[ 0 ] = 0xAA;
    CPUWriteComPorts[ 1 ] = 0xBB;
    SP = 0xEF;
    PC = 0xFFC0;
    memcpy( &APUMemory[ 0xFFC0 ], IPL_ROM, sizeof( IPL_ROM ) );
}

/* Execute next instruction, update PC and cycle counter etc */
void spc700Tick() {
    curr_program_counter = PC;
    uint8_t opcode = spcMemoryMapRead( PC++ );
    SPC700InstructionEntry *entry = &instructions[ opcode ];
    opCycles = entry->opCycles;
    next_program_counter = curr_program_counter + entry->opLength;
    if ( curr_program_counter == 0x086a ) {
        printf( "Debug break!\n" );
    }
    entry->instruction();
    // Operation may mutate next_program_counter if it branches/jumps
    PC = next_program_counter;

    updateTimers();
}

/* Access the 4 visible bytes from the CPU */
void spc700PortAccess( uint8_t addressBus, uint8_t *dataBus, bool writeLine ) {
    // 0x40->0x43, 0x44-0x7F (mirrors)
    // TODO - validate address bus

    uint8_t base = addressBus - 0x40;
    uint8_t port = base % 4;
    if ( writeLine ) {
        CPUWriteComPorts[ port ] = *dataBus;
    }
    else {
        *dataBus = *( ( &registers->port0 ) + port );
    }
}

/* 16 bit "register" from A and Y registers */
static inline uint16_t getYA() {
    return ( ( (uint16_t) Y ) << 8 ) | A;
}

static inline void storeYA( uint16_t YA ) {
    Y = ( YA >> 8 ) & 0x00FF;
    A = YA & 0x00FF;
}

static inline void spcRegisterAccess( uint16_t addressBus, uint8_t *dataBus, bool writeLine ) {

    uint8_t *hostAddress;
    // TODO - better access control
    if ( addressBus == 0xF0 ) {
        // Undocumented
        // TODO
        printf( "Accessing TEST register\n" );
        hostAddress = &APUMemory[ addressBus ];
    }
    else if ( addressBus == 0xF1 ) {
        // Control register
        if ( !writeLine ) {
            // TODO - error
            printf( "Attempting to read from control register\n" );
            *dataBus = 0x00;
            return;
        }
        // TODO - race conditions
        uint8_t val = *dataBus;
        if ( val & 0x80 ) {
            // READ IPL region goes to ROM
            // TODO
        }
        else {
            // READ IPL region goes to RAM
            // TODO
        }
        if ( val & 0x20 ) {
            // Clear PC32
            CPUWriteComPorts[ 3 ] = CPUWriteComPorts[ 2 ] = 0x00;
        }
        if ( val & 0x10 ) {
            // Clear PC10
            CPUWriteComPorts[ 1 ] = CPUWriteComPorts[ 0 ] = 0x00;
        }
        // Reset all timers
        timersStage2[ 0 ] = timersStage2[ 1 ] = timersStage2[ 2 ] = 0x00;
        registers->timer0counter = 0;
        registers->timer1counter = 0;
        registers->timer2counter = 0;

        t2Enabled = val & 0x04;
        t1Enabled = val & 0x02;
        t0Enabled = val & 0x01;
        
        return;
    }
    else if ( addressBus <= 0xF3 ) {
        if ( addressBus == 0x0F2 ) {
            hostAddress = &registers->dspAddress;
            accessDspAddressLatch( dataBus, writeLine );
        }
        else 
        {
            accessDspRegister( dataBus, writeLine );
        }
        return;
    }
    else if ( addressBus <= 0xF7 ) {
        // Communication ports        
        if ( writeLine ) {
            APUMemory[ addressBus ] = *dataBus;
        }
        else {
            // Need to read what main CPU wrote
            *dataBus = CPUWriteComPorts[ addressBus - 0xF4 ];
        }
        return;
    }
    else if ( addressBus <= 0xF9 ) {
        // Regular memory
        hostAddress = &APUMemory[ addressBus ];
    }
    else if ( addressBus <= 0xFC ) {
        // Sets registers->timer{0-2}
        if ( !writeLine ) {
            printf( "Attempting to read timer value\n" );
            *dataBus = 0x00;
        }
        else {
            APUMemory[ addressBus ] = *dataBus;
        }
        return;
    }
    else if ( addressBus <= 0xFF ) {
        // Counters
        if ( writeLine ) {
            printf( "Attempting to write to timer counter\n" );
        }
        else {
            *dataBus = APUMemory[ addressBus ] & 0x0F;
            APUMemory[ addressBus ] = 0x00;
        }
        return;
    }
    else {
        // Throw some error here
        hostAddress = NULL;
    }

    if ( writeLine ) {
        *hostAddress = *dataBus;
    }
    else {
        *dataBus = *hostAddress;
    }
}

// Might be better off breaking this into separate busRead and busWrite
/* static inline */ void spcBusAccess( uint16_t addressBus, uint8_t* dataBus, bool writeLine ) {
    // The if-block mapping can explicitly handle read-writes & return if required.
    // Otherwise, we fall through and do a regular memory write

    uint8_t *hostAddress = NULL;
    if ( addressBus <= 0x00EF ) {
        hostAddress = &APUMemory[ addressBus ];
    }
    else if ( addressBus <= 0x00FF ) {
        // Registers
        spcRegisterAccess( addressBus, dataBus, writeLine );
        return;
    }
    else if ( addressBus <= 0x01FF ) {
        // Page 1 and Memory
        hostAddress = &APUMemory[ addressBus ];
    }
    else { // if ( addr <= 0xFFFF )
        // Memory (Read/ReadWrite)/IPL Rom
        // TODO - adjust based on X reg
        hostAddress = &APUMemory[ addressBus ];
    }

    if ( writeLine ) {
        *hostAddress = *dataBus;
    }
    else {
        *dataBus = *hostAddress;
    }
}

/*static inline*/ uint8_t spcMemoryMapRead( uint16_t addr ) {
    uint8_t dataValue;
    spcBusAccess( addr, &dataValue, false );
    return dataValue;
}

// TODO - readU16 may need to know if page can increment, or just offset
/*static inline */uint16_t spcMemoryMapReadU16( uint16_t addr ){
    uint16_t dataValue = spcMemoryMapRead( addr );
    return dataValue | ( ( (uint16_t) spcMemoryMapRead( addr + 1 ) ) << 8 );
}

/*static inline */void spcMemoryMapWrite( uint16_t addr, uint8_t value ) {
    spcBusAccess( addr, &value, true );
}

// TODO - writeU16 may need to know if page can increment, or just offset
/*static inline */void spcMemoryMapWriteU16( uint16_t addr, uint16_t value ) {
    spcMemoryMapWrite( addr, (uint8_t)( value & 0x00FF ) );
    spcMemoryMapWrite( addr + 1, (uint8_t) ( value >> 8 ) );
} 

#pragma region SPC_ADDRESSING_MODES
static inline uint8_t immediate_new() {
    return spcMemoryMapRead( PC++ );
}
static inline uint16_t absolute_new() {
    uint16_t result = (uint16_t) immediate_new();
    result |= ( (uint16_t) immediate_new() ) << 8;
    return result;
}

static inline uint16_t absoluteX_new() {
    return absolute_new() + X;
}
static inline uint16_t absoluteY_new() {
    return absolute_new() + Y;
}

static inline uint16_t getPageAddr_new( uint8_t offset ) {
    uint16_t addr = (uint16_t)offset;
    if ( PSW & SPC_P_FLAG ) {
        addr += 0x0100;
    }
    return addr;
}
static inline uint16_t indirectX_new() {
    return getPageAddr_new( X );
}
static inline uint16_t indirectY_new() {
    return getPageAddr_new( Y );
}
static inline uint16_t direct_new( uint8_t offset ) {
    return getPageAddr_new( immediate_new() + offset );
}

static inline uint8_t absoluteMembit( uint16_t *addr ) {
    uint16_t operand = absolute_new();
    uint8_t bitLoc = ( operand >> 13 ) & 0x0007;
    *addr = operand & 0x1FFF;
    return bitLoc;
}

// DIRECT OPS START
#define DIRECT_D_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ) ) );

#define DIRECT_A_D_OP( op ) \
    op( A, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_A_D_WRITEOUT_OP( op ) \
    A = op( A, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_D_A_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );

#define DIRECT_X_D_WRITEOUT_OP( op ) \
    X = op( X, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_X_D_OP( op ) \
    op( X, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_D_X_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), X ) );

#define DIRECT_Y_D_WRITEOUT_OP( op ) \
    Y = op( Y, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_Y_D_OP( op ) \
    op( Y, spcMemoryMapRead( direct_new( 0 ) ) );

#define DIRECT_D_Y_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), Y ) );

#define DIRECT_YA_D_WRITEOUT_OP( op ) \
    uint16_t YA = getYA(); \
    YA = op( YA, spcMemoryMapRead( direct_new( 0 ) ) ); \
    storeYA( YA );

#define DIRECT_YA_D_OP( op ) \
    op( getYA(), spcMemoryMapRead( direct_new( 0 ) ) ); \
// DIRECT OPS END

// X-INDEXED DIRECT PAGE OPS START
#define X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( X ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ) ) );

#define X_INDEXED_DIRECT_PAGE_Y_DX_WRITEOUT_OP( op ) \
    Y = op( Y, spcMemoryMapRead( direct_new( X ) ) );

#define X_INDEXED_DIRECT_PAGE_DX_Y_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( X ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), Y ) );

#define X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( op ) \
    A = op( A, spcMemoryMapRead( direct_new( X ) ) );

#define X_INDEXED_DIRECT_PAGE_A_DX_OP( op ) \
    op( A, spcMemoryMapRead( direct_new( X ) ) );

#define X_INDEXED_DIRECT_PAGE_DX_A_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( X ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );

// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define X_INDEXED_DIRECT_PAGE_DX_R_OP( op ) \
    uint8_t nearLabel = immediate_new(); \
    uint8_t dpX = spcMemoryMapRead( direct_new( X ) ); \
    op( spcMemoryMapRead( dpX ), nearLabel );
// X-INDEXED DIRECT PAGE OPS END

// Y-INDEXED DIRECT PAGE OPS START
#define Y_INDEXED_DIRECT_PAGE_X_DY_WRITEOUT_OP( op ) \
    X = op( X, spcMemoryMapRead( direct_new( Y ) ) );

#define Y_INDEXED_DIRECT_PAGE_DY_X_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( Y ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), X ) );
// Y-INDEXED DIRECT PAGE OPS END

// INDIRECT OPS START
#define INDIRECT_A_X_WRITEOUT_OP( op ) \
    A = op( A, spcMemoryMapRead( indirectX_new() ) );

#define INDIRECT_A_X_OP( op ) \
    op( A, spcMemoryMapRead( indirectX_new() ) );

#define INDIRECT_X_A_WRITEOUT_OP( op ) \
    uint16_t addr = indirectX_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );
// INDIRECT OPS END

// INDIRECT AUTO INC OPS START
#define INDIRECT_AUTO_INC_X_A_WRITEOUT_OP( op ) \
    uint16_t addr = indirectX_new(); \
    ++X; \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) ); \

#define INDIRECT_AUTO_INC_A_X_WRITEOUT_OP( op ) \
    A = op( A, spcMemoryMapRead( indirectX_new() ) ); \
    ++X; \
// INDIRECT AUTO INC OPS END

// DIRECT PAGE TO DIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( op ) \
    uint8_t ds = spcMemoryMapRead( direct_new( 0 ) ); \
    uint16_t ddAddr = direct_new( 0 ); \
    spcMemoryMapWrite( ddAddr, op( spcMemoryMapRead( ddAddr ), ds ) ); \

#define DIRECT_PAGE_DIRECT_PAGE_OP( op ) \
    uint8_t ds = spcMemoryMapRead( direct_new( 0 ) ); \
    uint8_t dd = spcMemoryMapRead( direct_new( 0 ) ); \
    op( dd, ds ); \
// DIRECT PAGE TO DIRECT PAGE OPS END


// INDIRECT PAGE TO INDIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( op ) \
    uint8_t iY = spcMemoryMapRead( indirectY_new() ); \
    uint16_t iXAddr = indirectX_new(); \
    spcMemoryMapWrite( iXAddr, op( spcMemoryMapRead( iXAddr ), iY ) ); \

#define INDIRECT_PAGE_INDIRECT_PAGE_OP( op ) \
    uint8_t iY = spcMemoryMapRead( indirectY_new() ); \
    uint8_t iX = spcMemoryMapRead( indirectX_new() ); \
    op( iX, iY ); \
// INDIRECT PAGE TO INDIRECT PAGE OPS END

// IMMEDIATE TO DIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( op ) \
    uint8_t im = immediate_new(); \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), im ) );

#define IMMEDIATE_TO_DIRECT_PAGE_OP( op ) \
    uint8_t im = immediate_new(); \
    uint8_t dpVal = spcMemoryMapRead( direct_new( 0 ) ); \
    op( dpVal, im );
// IMMEDIATE TO DIRECT PAGE OPS END

// DIRECT PAGE BIT OPS START
#define DIRECT_PAGE_BIT_WRITEOUT_OP( op, bit ) \
    uint16_t addr = direct_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), 0x01 << bit ) );
// DIRECT PAGE BIT OPS END

// DIRECT PAGE BIT RELATIVE OPS START
// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define DIRECT_PAGE_BIT_RELATIVE_OP( op, bit ) \
    uint8_t d = spcMemoryMapRead( direct_new( 0 ) ); \
    uint8_t r = immediate_new(); \
    op( d & ( 0x01 << bit ), r ); 
// DIRECT PAGE BIT RELATIVE OPS END

// ABSOLUTE BOOLEAN BIT OPS START
// static inline uint8_t absoluteMembit( uint16_t *addr ) 

#define ABSOLUTE_BOOLEAN_BIT_MB_WRITEOUT_OP( op ) \
    uint16_t addr; \
    uint8_t mask = 1 << absoluteMembit( &addr ); \
    uint8_t MB = spcMemoryMapRead( addr ); \
    spcMemoryMapWrite( addr, op( MB & mask, MB, mask ) );
    
#define ABSOLUTE_BOOLEAN_BIT_C_MB_OP( op ) \
    uint16_t addr; \
    uint8_t mask = 1 << absoluteMembit( &addr ); \
    bool carrySet = PSW & SPC_CARRY_FLAG; \
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( op( carrySet, spcMemoryMapRead( addr ), mask ) ? SPC_CARRY_FLAG : 0x00 );

#define ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( op ) \
    uint16_t addr; \
    uint8_t mask = 1 << absoluteMembit( &addr ); \
    bool carrySet = PSW & SPC_CARRY_FLAG; \
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( op( carrySet, ~spcMemoryMapRead( addr ), mask ) ? SPC_CARRY_FLAG : 0x00 );

// TODO - will only work if O1 and O2 are commutative
#define ABSOLUTE_BOOLEAN_BIT_MB_C_WRITEOUT_OP( op ) \
    uint16_t addr; \
    uint8_t bit = absoluteMembit( &addr ); \
    uint8_t mask = 1 << absoluteMembit( &addr ); \
    bool carrySet = PSW & SPC_CARRY_FLAG; \
    spcMemoryMapWrite( addr, op( carrySet, spcMemoryMapRead( addr ), mask ) );

// ABSOLUTE BOOLEAN BIT OPS END

// ABSOLUTE OPS START
#define ABSOLUTE_a_addr_OP( op ) \
    op( absolute_new() );

#define ABSOLUTE_a_WRITEOUT_OP( op ) \
    uint16_t addr = absolute_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ) ) );

#define ABSOLUTE_A_a_WRITEOUT_OP( op ) \
    A = op( A, spcMemoryMapRead( absolute_new( 0 ) ) );

#define ABSOLUTE_A_a_OP( op ) \
    op( A, spcMemoryMapRead( absolute_new( 0 ) ) );

#define ABSOLUTE_a_A_WRITEOUT_OP( op ) \
    uint16_t addr = absolute_new( 0 ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );

#define ABSOLUTE_X_a_WRITEOUT_OP( op ) \
    X = op( X, spcMemoryMapRead( absolute_new( 0 ) ) );

#define ABSOLUTE_X_a_OP( op ) \
    op( X, spcMemoryMapRead( absolute_new( 0 ) ) );

#define ABSOLUTE_a_X_WRITEOUT_OP( op ) \
    uint16_t addr = absolute_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), X ) );

#define ABSOLUTE_Y_a_WRITEOUT_OP( op ) \
    Y = op( Y, spcMemoryMapRead( absolute_new() ) );

#define ABSOLUTE_Y_a_OP( op ) \
    op( Y, spcMemoryMapRead( absolute_new() ) );

#define ABSOLUTE_a_Y_WRITEOUT_OP( op ) \
    uint16_t addr = absolute_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), Y ) );
// ABSOLUTE OPS END

/* ABSOLUTE X-INDEXED INDIRECT OPS START
//#define ABSOLUTE_X_INDEXED_INDIRECT_ADDR_OP( op )\
//    op( absolute_addr() + X ); */
#define ABSOLUTE_X_INDEXED_INDIRECT_ADDR_OP( op )\
    op( spcMemoryMapReadU16( absoluteX_new() ) );
// ABSOLUTE X-INDEXED INDIRECT OPS END

// X-INDEXED ABSOLUTE OPS START
#define X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( op )\
    A = op( A, spcMemoryMapRead( absoluteX_new() ) );

#define X_INDEXED_ABSOLUTE_A_aX_OP( op )\
    op( A, spcMemoryMapRead( absoluteX_new() ) );

#define X_INDEXED_ABSOLUTE_aX_A_WRITEOUT_OP( op )\
    uint16_t addr = absoluteX_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );
// X-INDEXED ABSOLUTE OPS END

// Y-INDEXED ABSOLUTE OPS START
#define Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( op )\
    A = op( A, spcMemoryMapRead( absoluteY_new() ) );

#define Y_INDEXED_ABSOLUTE_A_aY_OP( op )\
    op( A, spcMemoryMapRead( absoluteY_new() ) );

#define Y_INDEXED_ABSOLUTE_aY_A_WRITEOUT_OP( op )\
    uint16_t addr = absoluteY_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );
// Y-INDEXED ABSOLUTE OPS END

// X-INDEXED INDIRECT OPS START
#define X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( op )\
    A = op( A, spcMemoryMapRead( spcMemoryMapReadU16( direct_new( X ) ) ) );

#define X_INDEXED_INDIRECT_A_dX_OP( op )\
    op( A, spcMemoryMapRead( spcMemoryMapReadU16( direct_new( X ) ) ) );

#define X_INDEXED_INDIRECT_dX_A_WRITEOUT_OP( op )\
    uint16_t addr = spcMemoryMapReadU16( direct_new( X ) ); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );
// X-INDEXED INDIRECT OPS END

// INDIRECT Y-INDEXED OPS START
#define INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( op )\
    A = op( A, spcMemoryMapRead( spcMemoryMapReadU16( direct_new( 0 ) ) + Y ) );

#define INDIRECT_Y_INDEXED_A_dY_OP( op )\
    op( A, spcMemoryMapRead( spcMemoryMapReadU16( direct_new( 0 ) ) + Y ) );

#define INDIRECT_Y_INDEXED_dY_A_WRITEOUT_OP( op )\
    uint16_t addr = spcMemoryMapReadU16( direct_new( 0 ) ) + Y; \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), A ) );
// INDIRECT Y-INDEXED OPS END

// RELATIVE OPS START

// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define RELATIVE_D_R_WRITEOUT_OP( op ) \
    uint16_t addr = direct_new( 0 ); \
    uint8_t r = immediate_new(); \
    spcMemoryMapWrite( addr, op( spcMemoryMapRead( addr ), r ) );

#define RELATIVE_OP_D_R( op ) \
    uint16_t addr = direct_new( 0 ); \
    uint8_t r = immediate_new(); \
    op( spcMemoryMapRead( addr ), r );

#define RELATIVE_Y_R_WRITEOUT_OP( op ) \
    Y = op( Y, immediate_new() );
// RELATIVE OPS END

// IMMEDIATE OPS START
#define IMMEDIATE_A_I_WRITEOUT_OP( op ) \
    A = op( A, immediate_new() );

#define IMMEDIATE_A_I_OP( op ) \
    op( A, immediate_new() );

#define IMMEDIATE_X_I_WRITEOUT_OP( op ) \
    X = op( X, immediate_new() );

#define IMMEDIATE_X_I_OP( op ) \
    op( X, immediate_new() );

#define IMMEDIATE_Y_I_WRITEOUT_OP( op ) \
    Y = op( Y, immediate_new() );

#define IMMEDIATE_Y_I_OP( op ) \
    op( Y, immediate_new() );
// IMMEDIATE OPS END

// IMPLIED OPS START
#define IMPLIED_REG_REG_WRITEOUT_OP( op, reg1, reg2 ) \
    reg1 = op( reg1, reg2 );

#define IMPLIED_A_WRITEOUT_OP( op ) \
    A = op( A )

#define IMPLIED_A_OP( op ) \
    op( A )

#define IMPLIED_X_WRITEOUT_OP( op ) \
    X = op( X );

#define IMPLIED_X_OP( op ) \
    op( X );

#define IMPLIED_Y_WRITEOUT_OP( op ) \
    Y = op( Y );

#define IMPLIED_Y_OP( op ) \
    op( Y );

#define IMPLIED_PSW_WRITEOUT_OP( op ) \
    PSW = op( PSW );

#define IMPLIED_PSW_OP( op ) \
    op( PSW );

#define IMPLIED_X_A_WRITEOUT_OP( op ) \
    X = op( X, A );

#define IMPLIED_A_X_WRITEOUT_OP( op ) \
    A = op( A, X );

#define IMPLIED_Y_A_WRITEOUT_OP( op ) \
    Y = op( Y, A );

#define IMPLIED_A_Y_WRITEOUT_OP( op ) \
    A = op( A, Y );

#define IMPLIED_SP_X_WRITEOUT_OP( op ) \
    SP = op( SP, X );

#define IMPLIED_X_SP_WRITEOUT_OP( op ) \
    X = op( X, SP );

#define IMPLIED_YA_OP( op ) \
    storeYA( op( getYA() ) ); \

#define IMPLIED_YA_X_OP( op ) \
    storeYA( op( getYA(), X ); );
// IMPLIED OPS END


#pragma endregion

#pragma region SPC_INSTRUCTIONS


#pragma region SPC_STACK
static inline uint8_t POP( uint8_t O1 ) {
    // TODO - remove O1
    UNUSED2( O1 );
    return spcMemoryMapRead( ++SP );
}

static inline void PUSH( uint8_t O1 ) {
    spcMemoryMapWrite( SP--, O1 );
}

static void fsAE_POP() {
    IMPLIED_A_WRITEOUT_OP( POP );
}
static void fs8E_POP() {
    IMPLIED_PSW_WRITEOUT_OP( POP );
}
static void fsCE_POP() {
    IMPLIED_X_WRITEOUT_OP( POP );
}
static void fsEE_POP() {
    IMPLIED_Y_WRITEOUT_OP( POP );
}
static void fs2D_PUSH() {
    IMPLIED_A_OP( PUSH );
}
static void fs0D_PUSH() {
    IMPLIED_PSW_OP( PUSH );
}
static void fs4D_PUSH() {
    IMPLIED_X_OP( PUSH );
}
static void fs6D_PUSH() {
    IMPLIED_Y_OP( PUSH );
}
#pragma endregion

#pragma region SPC_ADD_SBC

static uint8_t ADC( uint8_t O1, uint8_t O2 ) {
    uint8_t C = ( PSW & SPC_CARRY_FLAG ) ? 0x01 : 0x00;

    uint16_t result = ( (uint16_t)O1 ) + ( (uint16_t)O2 ) + ( (uint16_t)C );
    uint8_t result8 = (uint8_t)( result & 0x00FF );
    if ( (~( O1 ^ O2 ) & ( O1 ^ result8 ) ) & 0x80 ) {
        PSW |= SPC_OVERFLOW_FLAG;
    }
    if ( ( ( O1 & 0x0F ) + ( O2 & 0x0F ) + C ) > 0x0F ) {
        PSW |= SPC_HALF_CARRY_FLAG;
    }
    if ( result8 == 0 ){
        PSW |= SPC_ZERO_FLAG;
    }
    if ( result8 & 0x80 ){
        PSW |= SPC_NEGATIVE_FLAG;
    }
    if ( result > 0xFF ){
        PSW |= SPC_CARRY_FLAG;
    }
    return result8;
}

static uint8_t SBC( uint8_t O1, uint8_t O2 ) {
    uint8_t Bneg = ( ~( O2 + ( PSW & SPC_CARRY_FLAG ? 0 : 1 ) ) ) + 1;
    return ADC( O1, Bneg );
}

static void fs99_ADC() {
    INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( ADC );
}
static void fs88_ADC() {
    IMMEDIATE_A_I_WRITEOUT_OP( ADC );
}
static void fs86_ADC() {
    INDIRECT_X_A_WRITEOUT_OP( ADC );
}
static void fs97_ADC() {
    INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( ADC );
}
static void fs87_ADC() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( ADC );
}
static void fs84_ADC() {
    DIRECT_A_D_WRITEOUT_OP( ADC );
}
static void fs94_ADC() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( ADC );
}
static void fs85_ADC() {
    ABSOLUTE_A_a_WRITEOUT_OP( ADC );
}
static void fs95_ADC() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( ADC );
}
static void fs96_ADC() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( ADC );
}
static void fs89_ADC() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( ADC );
}
static void fs98_ADC() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( ADC );
}
static void fs7A_ADDW() {
    // TODO
    uint16_t YA = getYA();
    uint16_t O1 = spcMemoryMapRead( direct_new( 0 ) );
    uint32_t result = (uint32_t)YA + (uint32_t)O1;
    PSW &= ~( SPC_NEGATIVE_FLAG | SPC_OVERFLOW_FLAG | SPC_HALF_CARRY_FLAG | SPC_ZERO_FLAG | SPC_CARRY_FLAG );
    PSW = PSW
        | ( ( ( ~( YA ^ O1 ) & ( YA ^ result ) ) & 0x8000 ) ? SPC_OVERFLOW_FLAG : 0x00 )
        | ( ( result > 0xFFFF ) ? SPC_CARRY_FLAG : 0x00 )
        | ( ( ( ( YA & 0x0FFF ) + ( O1 & 0x0FFF ) ) > 0x0FFF ) ? SPC_HALF_CARRY_FLAG : 0x00 )
        | ( ( (uint16_t) result == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( result & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 );
    storeYA( YA );
}

static void fsB9_SBC() {
    INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( SBC );
}
static void fsA8_SBC() {
    IMMEDIATE_A_I_WRITEOUT_OP( SBC );
}
static void fsA6_SBC() {
    INDIRECT_A_X_WRITEOUT_OP( SBC );
}
static void fsB7_SBC() {
    INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( SBC );
}
static void fsA7_SBC() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( SBC );
}
static void fsA4_SBC() {
    DIRECT_A_D_WRITEOUT_OP( SBC );
}
static void fsB4_SBC() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( SBC );
}
static void fsA5_SBC() {
    ABSOLUTE_A_a_WRITEOUT_OP( SBC );
}
static void fsB5_SBC() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( SBC );
}
static void fsB6_SBC() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( SBC );
}
static void fsA9_SBC() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( SBC );
}
static void fsB8_SBC() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( SBC );
}
static void fs9A_SUBW() {
    // TODO
    uint32_t YA = (uint32_t) getYA();
    uint32_t O1 = (uint32_t) spcMemoryMapReadU16( direct_new( 0 ) );

    uint32_t result = YA - O1;
    bool overflow = ( ( YA ^ O1 ) & ( YA ^ result ) ) & 0x8000;
    bool carry = ( result <= 0xFFFF );
    
    uint16_t temp = ( ( YA & 0x0F00 ) - ( O1 & 0x0F00 ) ) >> 8;
    if ( ( YA & 0xFF ) < ( O1 & 0xFF ) ) {
      --temp;
    }
    
    storeYA( (uint8_t)( result & 0x0000FFFF ) );

    PSW &= ~( SPC_NEGATIVE_FLAG | SPC_OVERFLOW_FLAG | SPC_HALF_CARRY_FLAG | SPC_ZERO_FLAG | SPC_CARRY_FLAG );
    PSW = PSW
        | ( overflow ? SPC_OVERFLOW_FLAG : 0x00 )
        | ( carry ? SPC_CARRY_FLAG : 0x00 )
        | ( ( temp <= 0x0F ) ? SPC_HALF_CARRY_FLAG : 0x00 )
        | ( ( ( result & 0x0000FFFF ) == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( result & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

#pragma endregion 

#pragma region SPC_AND
static inline uint8_t AND( uint8_t O1, uint8_t O2 ) {
    O1 = O1 & O2;

    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 ) 
        | ( ( O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );

    return O1;
}

static uint8_t AND1( bool O1, uint8_t O2, uint8_t O2Mask ) {
    bool O2S = O2 & O2Mask;
    return ( O1 && O2Mask ) ? O2Mask : 0x00;
}

static void fs39_AND() {
    INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( AND );
}
static void fs28_AND() {
    IMMEDIATE_A_I_WRITEOUT_OP( AND );
}
static void fs26_AND() {
    INDIRECT_A_X_WRITEOUT_OP( AND );
}
static void fs37_AND() {
    INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( AND );
}
static void fs27_AND() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( AND );
}
static void fs24_AND() {
    DIRECT_A_D_WRITEOUT_OP( AND );
}
static void fs34_AND() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( AND );
}
static void fs25_AND() {
    ABSOLUTE_A_a_WRITEOUT_OP( AND );
}
static void fs35_AND() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( AND );
}
static void fs36_AND() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( AND );
}
static void fs29_AND() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( AND );
}
static void fs38_AND() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( AND );
}
static void fs6A_AND1() {
    ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( AND1 );
}
static void fs4A_AND1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( AND1 );
}

#pragma endregion 

#pragma region SPC_BIT_SHIFT
static inline uint8_t LSR( uint8_t O1 ) {
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( O1 & 0x01 ) ? SPC_CARRY_FLAG : 0x00 );
    O1 = ( O1 >> 1 ) & ~0x7F;

    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( O1 == 0x00 ) ? SPC_ZERO_FLAG : 0x00 );
    return O1;
}
static inline uint8_t ASL( uint8_t O1 ) {
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( O1 & 0x80 ) ? SPC_CARRY_FLAG : 0x00 );
    O1 = ( O1 << 1 ) & 0xFE;

    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( O1 == 0x00 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );

    return O1;
}

static void fs1C_ASL() {
    IMPLIED_A_WRITEOUT_OP( ASL );
}
static void fs0B_ASL() {
    DIRECT_D_WRITEOUT_OP( ASL );
}
static void fs1B_ASL() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( ASL );
}
static void fs0C_ASL() {
    ABSOLUTE_a_WRITEOUT_OP( ASL );
}
static void fs5C_LSR() {
    IMPLIED_A_WRITEOUT_OP( LSR );
}
static void fs4B_LSR() {
    DIRECT_D_WRITEOUT_OP( LSR );
}
static void fs5B_LSR() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( LSR );
}
static void fs4C_LSR() {
    ABSOLUTE_a_WRITEOUT_OP( LSR );
}

static uint8_t ROL( uint8_t O1 ) {
    uint8_t lsb = PSW & SPC_CARRY_FLAG ? 0x01 : 0x00;
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( O1 & 0x80 ) ? SPC_CARRY_FLAG : 0x00 );
    return ( ( O1 << 1 ) & 0xFE ) | lsb;
}

static uint8_t ROR( uint8_t O1 ) {
    uint8_t msb = PSW & SPC_CARRY_FLAG ? 0x80 : 0x00;
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( O1 & 0x01 ) ? SPC_CARRY_FLAG : 0x00 );
    return ( ( O1 >> 1 ) & 0x7F ) | msb;
}

static void fs3C_ROL() {
    IMPLIED_A_WRITEOUT_OP( ROL );
}
static void fs2B_ROL() {
    DIRECT_D_WRITEOUT_OP( ROL );
}
static void fs3B_ROL() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( ROL );
}
static void fs2C_ROL() {
    ABSOLUTE_a_WRITEOUT_OP( ROL );
}
static void fs7C_ROR() {
    IMPLIED_A_WRITEOUT_OP( ROR );
}
static void fs6B_ROR() {
    DIRECT_D_WRITEOUT_OP( ROR );
}
static void fs7B_ROR() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( ROR );
}
static void fs6C_ROR() {
    ABSOLUTE_a_WRITEOUT_OP( ROR );
}

#pragma endregion

#pragma region SPC_BRANCH

static void BranchOnCondition( bool condition, int8_t offset ) {
    if ( condition ) {
        next_program_counter = next_program_counter + (int8_t)offset;
        opCycles += 2;
    }
}

static void BranchImmediateOnCondition( bool condition ) {
    BranchOnCondition( condition, (int8_t) immediate_new() );
}

static void BBC( bool O1, uint8_t r ) {
    BranchOnCondition( !O1, (int8_t) r );
}

static void BBS( bool O1, uint8_t r ) {
    BranchOnCondition( O1, (int8_t) r );
}

static void fs13_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 0 );
}
static void fs33_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 1 );
}
static void fs53_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 2 );
}
static void fs73_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 3 );
}
static void fs93_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 4 );
}
static void fsB3_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 5 );
}
static void fsD3_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 6 );
}
static void fsF3_BBC() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBC, 7 );
}

static void fs03_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 0 );
}
static void fs23_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 1 );
}
static void fs43_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 2 );
}
static void fs63_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 3 );
}
static void fs83_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 4 );
}
static void fsA3_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 5 );
}
static void fsC3_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 6 );
}
static void fsE3_BBS() {
    DIRECT_PAGE_BIT_RELATIVE_OP( BBS, 7 );
}

static void fs90_BCC() {
    BranchImmediateOnCondition( !( PSW & SPC_CARRY_FLAG ) );
}
static void fsB0_BCS() {
    BranchImmediateOnCondition( PSW & SPC_CARRY_FLAG );
}
static void fsF0_BEQ() {
    BranchImmediateOnCondition( PSW & SPC_ZERO_FLAG );
}
static void fs30_BMI() {
    BranchImmediateOnCondition( PSW & SPC_NEGATIVE_FLAG );
}
static void fsD0_BNE() {
    BranchImmediateOnCondition( !( PSW & SPC_ZERO_FLAG ) );
}
static void fs10_BPL() {
    BranchImmediateOnCondition( !( PSW & SPC_NEGATIVE_FLAG ) );
}
static void fs50_BVC() {
    BranchImmediateOnCondition( !( PSW & SPC_OVERFLOW_FLAG ) );
}
static void fs70_BVS() {
    BranchImmediateOnCondition( PSW & SPC_OVERFLOW_FLAG );
}
static void fs2F_BRA() {
    BranchImmediateOnCondition( true );
    opCycles -= 2;
}

static void JMP( uint16_t addr ) {
    next_program_counter = addr;
}

static void fs0F_BRK() {
    // TODO
    PSW |= SPC_BREAK_FLAG;
    uint8_t r = ( next_program_counter >> 8 ) & 0x00FF;
    PUSH( r );
    r = next_program_counter & 0x00FF;
    PUSH( r );
    PUSH( PSW );

    PSW &= ~SPC_INTERRUPT_FLAG;
    JMP( 0xFFDE );
}

static void fs1F_JMP() {
    ABSOLUTE_X_INDEXED_INDIRECT_ADDR_OP( JMP );
}

static void fs5F_JMP() {
    ABSOLUTE_a_addr_OP( JMP );
}

#pragma endregion 

#pragma region SPC_CLR
static uint8_t CLR( uint8_t O1, uint8_t mask ) {
    return O1 & ~mask;
}
static void fs12_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 0 );
}
static void fs32_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 1 );
}
static void fs52_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 2 );
}
static void fs72_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 3 );
}
static void fs92_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 4 );
}
static void fsB2_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 5 );
}
static void fsD2_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 6 );
}
static void fsF2_CLR1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( CLR, 7 );
}
static void fs60_CLRC() {
    PSW &= ~SPC_CARRY_FLAG;
}
static void fs20_CLRP() {
    PSW &= ~SPC_P_FLAG;
}
static void fsE0_CLRV() {
    PSW &= ~SPC_BREAK_FLAG;
}
#pragma endregion 

#pragma region SPC_CMP
static void CMP( uint8_t O1, uint8_t O2 ) {
    // TODO - verify
    int8_t sO1 = (int8_t)O1;
    int8_t sO2 = (int8_t)O2;
    int8_t result = sO1 - sO2;
    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_CARRY_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( result == 0 ) ? SPC_ZERO_FLAG
        : ( ( ( sO1 >= sO2 ) ? SPC_CARRY_FLAG : 0x00 )
        | ( ( result & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 ) ) );
}
static void CMPW( uint16_t O1, uint8_t O2 ) {
    // TODO - verify
    int16_t sO1 = (int16_t)O1;
    int16_t sO2 = (int16_t)O2;
    int16_t result = sO1 - sO2;
    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( result == 0 ) ? SPC_ZERO_FLAG
        : ( ( result & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 ) );
}

static void fs79_CMP() {
    INDIRECT_PAGE_INDIRECT_PAGE_OP( CMP );
}
static void fs68_CMP() {
    IMMEDIATE_A_I_OP( CMP );
}
static void fs66_CMP() {
    INDIRECT_A_X_OP( CMP );
}
static void fs77_CMP() {
    INDIRECT_Y_INDEXED_A_dY_OP( CMP );
}
static void fs67_CMP() {
    X_INDEXED_INDIRECT_A_dX_OP( CMP );
}
static void fs64_CMP() {
    DIRECT_A_D_OP( CMP );
}
static void fs74_CMP() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( CMP );
}
static void fs65_CMP() {
    ABSOLUTE_A_a_OP( CMP );
}
static void fs75_CMP() {
    X_INDEXED_ABSOLUTE_A_aX_OP( CMP );
}
static void fs76_CMP() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( CMP );
}
static void fsC8_CMP() {
    IMMEDIATE_X_I_OP( CMP );
}
static void fs3E_CMP() {
    DIRECT_X_D_OP( CMP );
}
static void fs1E_CMP() {
    ABSOLUTE_X_a_OP( CMP );
}
static void fsAD_CMP() {
    IMMEDIATE_Y_I_OP( CMP );
}
static void fs7E_CMP() {
    DIRECT_Y_D_OP( CMP );
}
static void fs5E_CMP() {
    ABSOLUTE_Y_a_OP( CMP );
}
static void fs69_CMP() {
    DIRECT_PAGE_DIRECT_PAGE_OP( CMP );
}
static void fs78_CMP() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( CMP );
}
static void fs5A_CMPW() {
    DIRECT_YA_D_OP( CMPW );
}
#pragma endregion

#pragma region SPC_DEC_INC
static inline uint8_t DEC( uint8_t O1 ) {
    --O1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    return O1;
}
static inline uint8_t INC( uint8_t O1 ) {
    ++O1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    return O1;
}
static void fs9C_DEC() {
    IMPLIED_A_WRITEOUT_OP( DEC );
}
static void fs1D_DEC() {
    IMPLIED_X_WRITEOUT_OP( DEC );
}
static void fsDC_DEC() {
    IMPLIED_Y_WRITEOUT_OP( DEC );
}
static void fs8B_DEC() {
    DIRECT_D_WRITEOUT_OP( DEC );
}
static void fs9B_DEC() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( DEC );
}
static void fs8C_DEC() {
    ABSOLUTE_a_WRITEOUT_OP( DEC );
}
static void fs1A_DECW() {
    uint16_t addr = direct_new( 0 );
    uint16_t O1 = spcMemoryMapReadU16( addr );
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( O1 & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    
    spcMemoryMapWriteU16( addr, O1 );
}
static void fsBC_INC() {
    IMPLIED_A_WRITEOUT_OP( INC );
}
static void fs3D_INC() {
    IMPLIED_X_WRITEOUT_OP( INC );
}
static void fsFC_INC() {
    IMPLIED_Y_WRITEOUT_OP( INC );
}
static void fsAB_INC() {
    DIRECT_D_WRITEOUT_OP( INC );
}
static void fsBB_INC() {
    X_INDEXED_DIRECT_PAGE_DX_WRITEOUT_OP( INC );
}
static void fsAC_INC() {
    ABSOLUTE_a_WRITEOUT_OP( INC );
}
static void fs3A_INCW() {
    uint16_t addr = direct_new( 0 );
    uint16_t O1 = spcMemoryMapReadU16( addr );
    ++O1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( O1 & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    
    spcMemoryMapWriteU16( addr, O1 );
}
#pragma endregion 

#pragma region SPC_EOR_OR
static inline uint8_t EOR( uint8_t O1, uint8_t O2 ) {
    // TODO - flags?
    return O1 ^ O2;
}
static inline uint8_t EOR1( bool O1, uint8_t O2, uint8_t mask ) {
    bool O2S = O2 & mask;
    return ( O1 ^ O2S ) ? mask : 0x00;
}
static inline uint8_t OR( uint8_t O1, uint8_t O2 ) {
    // TODO - flags?
    return O1 | O2;
}
static inline uint8_t OR1( bool O1, uint8_t O2, uint8_t mask ) {
    bool O2S = O2 & mask;
    return ( O1 || O2S ) ? mask : 0x00;
}

static void fs59_EOR() {
    INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( EOR );
}
static void fs48_EOR() {
    IMMEDIATE_A_I_WRITEOUT_OP( EOR );
}
static void fs46_EOR() {
    INDIRECT_A_X_WRITEOUT_OP( EOR );
}
static void fs57_EOR() {
   INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( EOR );
}
static void fs47_EOR() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( EOR );
}
static void fs44_EOR() {
    DIRECT_A_D_WRITEOUT_OP( EOR );
}
static void fs54_EOR() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( EOR );
}
static void fs45_EOR() {
    ABSOLUTE_A_a_WRITEOUT_OP( EOR );
}
static void fs55_EOR() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( EOR );
}
static void fs56_EOR() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( EOR );
}
static void fs49_EOR() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( EOR );
}
static void fs58_EOR() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( EOR );
}
static void fs8A_EOR1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( EOR1 );
}

static void fs19_OR() {
    INDIRECT_PAGE_INDIRECT_PAGE_WRITEOUT_OP( OR );
}
static void fs08_OR() {
    IMMEDIATE_A_I_WRITEOUT_OP( OR );
}
static void fs06_OR() {
    INDIRECT_A_X_WRITEOUT_OP( OR );
}
static void fs17_OR() {
   INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( OR );
}
static void fs07_OR() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( OR );
}
static void fs04_OR() {
    DIRECT_A_D_WRITEOUT_OP( OR );
}
static void fs14_OR() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( OR );
}
static void fs05_OR() {
    ABSOLUTE_A_a_WRITEOUT_OP( OR );
}
static void fs15_OR() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( OR );
}
static void fs16_OR() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( OR );
}
static void fs09_OR() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( OR );
}
static void fs18_OR() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( OR );
}
static void fs2A_OR1() {
    ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( OR1 );
}
static void fs0A_OR1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( OR1 );
}
#pragma endregion 

#pragma region SPC_MOV
// TODO - Add the extra read-cycle for some of these
// TODO - these only really need 1 parameter
static inline uint8_t MOV( uint8_t O1, uint8_t O2 ) {
    O1 = O2;
    return O1;
}

static inline uint8_t MOV_FLAG( uint8_t O1, uint8_t O2 ) {
    O1 = O2;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( ( O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );

    return O1;
}

static inline uint8_t MOV1( bool O1, uint8_t O2, uint8_t mask ) {
    O1 = ( O2 & mask ); // TODO - disable warning on unused O1
    return O1 ? mask : 0x00;
}

static void fsAF_MOV() {
    INDIRECT_AUTO_INC_X_A_WRITEOUT_OP( MOV );
}
static void fsC6_MOV() {
    INDIRECT_X_A_WRITEOUT_OP( MOV );
}
static void fsD7_MOV() {
    INDIRECT_Y_INDEXED_dY_A_WRITEOUT_OP( MOV )
}
static void fsC7_MOV() {
    X_INDEXED_INDIRECT_dX_A_WRITEOUT_OP( MOV );
}
static void fsE8_MOV() {
    IMMEDIATE_A_I_WRITEOUT_OP( MOV_FLAG );
}
static void fsE6_MOV() {
    INDIRECT_A_X_WRITEOUT_OP( MOV_FLAG );
}
static void fsBF_MOV() {
    INDIRECT_AUTO_INC_A_X_WRITEOUT_OP( MOV_FLAG );
}
static void fsF7_MOV() {
   INDIRECT_Y_INDEXED_A_dY_WRITEOUT_OP( MOV_FLAG );
}
static void fsE7_MOV() {
    X_INDEXED_INDIRECT_A_dX_WRITEOUT_OP( MOV_FLAG );
}
static void fs7D_MOV() {
    IMPLIED_A_X_WRITEOUT_OP( MOV_FLAG );
}
static void fsDD_MOV() {
    IMPLIED_A_Y_WRITEOUT_OP( MOV_FLAG );
}
static void fsE4_MOV() {
    DIRECT_A_D_WRITEOUT_OP( MOV_FLAG );
}
static void fsF4_MOV() {
    X_INDEXED_DIRECT_PAGE_A_DX_WRITEOUT_OP( MOV_FLAG );
}
static void fsE5_MOV() {
    ABSOLUTE_A_a_WRITEOUT_OP( MOV_FLAG );
}
static void fsF5_MOV() {
    X_INDEXED_ABSOLUTE_A_aX_WRITEOUT_OP( MOV_FLAG );
}
static void fsF6_MOV() {
    Y_INDEXED_ABSOLUTE_A_aY_WRITEOUT_OP( MOV_FLAG );
}
static void fsBD_MOV() {
    IMPLIED_SP_X_WRITEOUT_OP( MOV );
}
static void fsCD_MOV() {
    IMMEDIATE_X_I_WRITEOUT_OP( MOV_FLAG );
}
static void fs5D_MOV() {
    IMPLIED_X_A_WRITEOUT_OP( MOV_FLAG );
}
static void fs9D_MOV() {
    IMPLIED_X_SP_WRITEOUT_OP( MOV_FLAG );
}
static void fsF8_MOV() {
    DIRECT_X_D_WRITEOUT_OP( MOV_FLAG );
}
static void fsF9_MOV() {
    Y_INDEXED_DIRECT_PAGE_X_DY_WRITEOUT_OP( MOV_FLAG );
}
static void fsE9_MOV() {
    ABSOLUTE_X_a_WRITEOUT_OP( MOV_FLAG );
}
static void fs8D_MOV() {
    IMMEDIATE_Y_I_WRITEOUT_OP( MOV_FLAG );
}
static void fsFD_MOV() {
    IMPLIED_Y_A_WRITEOUT_OP( MOV_FLAG );
}
static void fsEB_MOV() {
    DIRECT_Y_D_WRITEOUT_OP( MOV_FLAG );
}
static void fsFB_MOV() {
    X_INDEXED_DIRECT_PAGE_Y_DX_WRITEOUT_OP( MOV_FLAG );
}
static void fsEC_MOV() {
    ABSOLUTE_Y_a_WRITEOUT_OP( MOV_FLAG );
}
static void fsFA_MOV() {
    DIRECT_PAGE_DIRECT_PAGE_WRITEOUT_OP( MOV );
}
static void fsD4_MOV() {
    X_INDEXED_DIRECT_PAGE_DX_A_WRITEOUT_OP( MOV );
}
static void fsDB_MOV() {
    X_INDEXED_DIRECT_PAGE_DX_Y_WRITEOUT_OP( MOV );
}
static void fsD9_MOV() {
    Y_INDEXED_DIRECT_PAGE_DY_X_WRITEOUT_OP( MOV );
}
static void fs8F_MOV() {
    IMMEDIATE_TO_DIRECT_PAGE_WRITEOUT_OP( MOV );
}
static void fsC4_MOV() {
    DIRECT_D_A_WRITEOUT_OP( MOV );
}
static void fsD8_MOV() {
    DIRECT_D_X_WRITEOUT_OP( MOV );
}
static void fsCB_MOV() {
    DIRECT_D_Y_WRITEOUT_OP( MOV );
}
static void fsD5_MOV() {
    X_INDEXED_ABSOLUTE_aX_A_WRITEOUT_OP( MOV );
}
static void fsD6_MOV() {
    Y_INDEXED_ABSOLUTE_aY_A_WRITEOUT_OP( MOV );
}
static void fsC5_MOV() {
    ABSOLUTE_a_A_WRITEOUT_OP( MOV );
}
static void fsC9_MOV() {
    ABSOLUTE_a_X_WRITEOUT_OP( MOV );
}
static void fsCC_MOV() {
    ABSOLUTE_a_Y_WRITEOUT_OP( MOV );
}
static void fsAA_MOV1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( MOV1 );
}
static void fsCA_MOV1() {
    ABSOLUTE_BOOLEAN_BIT_MB_C_WRITEOUT_OP( MOV1 );
}
static void fsBA_MOVW() {
    //DIRECT_YA_D_OP( MOVW_YA_D );
    uint16_t YA = spcMemoryMapReadU16( direct_new( 0 ) );
    storeYA( YA );
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( ( YA & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( YA == 0 ) ? SPC_ZERO_FLAG : 0x00 );
}
static void fsDA_MOVW() {
    //DIRECT_D_YA_OP( MOVW_YA_D );
    spcMemoryMapWriteU16( direct_new( 0 ), getYA() );
}

#pragma endregion

#pragma region SPC_SET
static inline uint8_t SET( uint8_t O1, uint8_t mask ) {
    return O1 | mask;
}
static void fs02_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 0 );
}
static void fs22_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 1 );
}
static void fs42_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 2 );
}
static void fs62_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 3 );
}
static void fs82_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 4 );
}
static void fsA2_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 5 );
}
static void fsC2_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 6 );
}
static void fsE2_SET1() {
    DIRECT_PAGE_BIT_WRITEOUT_OP( SET, 7 );
}
static void fs80_SETC() {
    PSW |= SPC_CARRY_FLAG;
}
static void fs40_SETP() {
    PSW |= SPC_P_FLAG;
}
#pragma endregion

#pragma region SPC_TCALL
static inline void TCALL( uint16_t addr ) {
    JMP( addr );
}
static void fs01_TCALL() {
    TCALL( 0xFFDE );
}
static void fs11_TCALL() {
    TCALL( 0xFFDC );
}
static void fs21_TCALL() {
    TCALL( 0xFFDA );
}
static void fs31_TCALL() {
    TCALL( 0xFFD8 );
}
static void fs41_TCALL() {
    TCALL( 0xFFD6 );
}
static void fs51_TCALL() {
    TCALL( 0xFFD4 );
}
static void fs61_TCALL() {
    TCALL( 0xFFD2 );
}
static void fs71_TCALL() {
    TCALL( 0xFFD0 );
}
static void fs81_TCALL() {
    TCALL( 0xFFCE );
}
static void fs91_TCALL() {
    TCALL( 0xFFCC );
}
static void fsA1_TCALL() {
    TCALL( 0xFFCA );
}
static void fsB1_TCALL() {
    TCALL( 0xFFC8 );
}
static void fsC1_TCALL() {
    TCALL( 0xFFC6 );
}
static void fsD1_TCALL() {
    TCALL( 0xFFC4 );
}
static void fsE1_TCALL() {
    TCALL( 0xFFC2 );
}
static void fsF1_TCALL() {
    TCALL( 0xFFC0 );
}
#pragma endregion

#pragma region SPC_UNCATEGORISED
static void fs3F_CALL() {
    uint8_t r = ( next_program_counter >> 8 ) & 0x00FF;
    PUSH( r );
    r = next_program_counter & 0x00FF;
    PUSH( r );
    next_program_counter = absolute_new();
    ++PC;
}

static void CBNE( uint8_t O1, uint16_t R ) {
    if ( A == O1 ){
        next_program_counter += (int8_t)R;
        opCycles += 2;
    }
}
static void fsDE_CBNE() {
    X_INDEXED_DIRECT_PAGE_DX_R_OP( CBNE );
}
static void fs2E_CBNE() {
    RELATIVE_OP_D_R( CBNE );
}
static void fsDF_DAA() {
    // TODO - verify these
    if ( ( PSW & SPC_CARRY_FLAG ) || ( A > 0x99 ) ) {
       PSW |= SPC_CARRY_FLAG;
       A += 0x60;
    }
    
    if ( ( PSW & SPC_HALF_CARRY_FLAG ) || ( A & 0x0F ) > 0x09 ) {
       A += 0x06;
    }

    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( A == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( A & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );

}
static void fsBE_DAS() {
    if ( !( PSW & SPC_CARRY_FLAG ) || ( A > 0x99 ) ) {
       PSW &= ~SPC_CARRY_FLAG;
       A -= 0x60;
    }
    
    if ( !( PSW & SPC_HALF_CARRY_FLAG ) || ( A & 0x0F ) > 0x09 ) {
       A -= 0x06;
    }
  
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( A == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( A & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static inline uint8_t DBNZ( uint8_t O1, uint8_t r ) {
    --O1;
    if ( O1 != 0 ) {
        next_program_counter = next_program_counter + (int8_t)r;
        opCycles += 1;
    }
    return O1;
}
static void fsFE_DBNZ() {
    RELATIVE_Y_R_WRITEOUT_OP( DBNZ );
}
static void fs6E_DBNZ() {
    RELATIVE_D_R_WRITEOUT_OP( DBNZ )
}

static void fs9E_DIV() {
    // TODO
    uint16_t YA = getYA();
    bool overflow = ( Y >= X );
    bool halfCarry = ( Y & 0x0F ) >= ( X & 0x0F );
    if (Y < (X << 1)) {
        A = YA / X;
        Y = YA % X;
    } else {
        A = 255 - ( YA - ( X << 9 ) ) / ( 256 - X );
        Y = X   + ( YA - ( X << 9 ) ) % ( 256 - X );
    }

    PSW &= ~( SPC_OVERFLOW_FLAG | SPC_HALF_CARRY_FLAG | SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG );
    PSW = PSW 
            | ( overflow ? SPC_OVERFLOW_FLAG : 0x00 )
            | ( halfCarry ? SPC_HALF_CARRY_FLAG : 0x00 )
            | ( ( A == 0 ) ? SPC_ZERO_FLAG : 0x00 )
            | ( ( A & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}
static void fsC0_DI() {
    PSW &= ~SPC_INTERRUPT_FLAG;
}
static void fsA0_EI() {
    PSW |= SPC_INTERRUPT_FLAG;
}
static void fsCF_MUL() {
    // TODO
    uint16_t YxA = Y * A;
    storeYA( YxA );
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
            | ( ( Y == 0 ) ? SPC_ZERO_FLAG : 0x00 )
            | ( ( Y & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static void fs00_NOP() {
}

// TODO - might be able to simplify this
static uint8_t NOT1( bool O1, uint8_t value, uint8_t mask ) {
    UNUSED2( O1 );
    return value ^ mask;
}

static void fsEA_NOT1() {
    ABSOLUTE_BOOLEAN_BIT_MB_WRITEOUT_OP( NOT1 );
}
static void fsED_NOTC() {
    PSW = PSW ^ SPC_CARRY_FLAG;
}

static void fs4F_PCALL() {
    next_program_counter = 0xFF00 + immediate_new();
}
static void fs6F_RET() {
    uint8_t h, l;
    l = POP( 0 );
    h = POP( 0 );
    next_program_counter = ( ( (uint16_t)h ) << 8 ) | (uint16_t) l;
}
static void fs7F_RET1() {
    PSW = POP( 0 );
    fs6F_RET();
}
static void fsEF_SLEEP() {
    // TODO
}

static void fsFF_STOP() {
    // TODO
}

static void fs4E_TCLR1() {
    uint16_t addr = absolute_new();
    uint8_t value = spcMemoryMapRead( addr );
    uint8_t temp = value & A;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( temp & 0x80 ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( temp == 0 ? SPC_ZERO_FLAG : 0x00 );

    spcMemoryMapWrite( addr, value & ~A );
}
static void fs0E_TSET1() {
    uint16_t addr = absolute_new();
    uint8_t value = spcMemoryMapRead( addr );
    uint8_t temp = value & A;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( temp & 0x80 ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( temp == 0 ? SPC_ZERO_FLAG : 0x00 );
        
    spcMemoryMapWrite( addr, value | A );
}
static void fs9F_XCN() {
    A = ( A >> 4 ) | ( A << 4 );
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( A & 0x80 ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( A == 0 ? SPC_ZERO_FLAG : 0x00 );
}

#pragma endregion

/* Populate the instruction functions array */
SPC700InstructionEntry instructions[ 0x100 ] = 
{
    { fs00_NOP, 1, 2 },     // ........ :          : do nothing
    { fs01_TCALL, 1, 8 },   // ........ : 0        : CALL [$FFDE]
    { fs02_SET1, 2, 4 },    // ........ : d.0      : d.0 = 1
    { fs03_BBS, 3, 5 },     // ........ : d.0, r   : PC+=r  if d.0 == 1
    { fs04_OR, 2, 3 },      // N.....Z. : A, d     : A = A | (d)
    { fs05_OR, 3, 4 },      // N.....Z. : A, !a    : A = A | (a)
    { fs06_OR, 1, 3 },      // N.....Z. : A, (X)   : A = A | (X)
    { fs07_OR, 2, 6 },      // N.....Z. : A, [d+X] : A = A | ([d+X])
    { fs08_OR, 2, 2 },      // N.....Z. : A, #i    : A = A | i
    { fs09_OR, 3, 6 },      // N.....Z. : dd, ds   : (dd) = (dd) | (ds)
    { fs0A_OR1, 3, 5 },     // .......C : C, m.b   : C = C | (m.b)
    { fs0B_ASL, 2, 4 },     // N.....ZC : d        : Left shift (d) as above
    { fs0C_ASL, 3, 5 },     // N.....ZC : !a       : Left shift (a) as above
    { fs0D_PUSH, 1, 4 },	// ........ : PSW      : (SP--) = Flags
    { fs0E_TSET1, 3, 6 },	// N.....Z. : !a       : (a) = (a)|A, ZN as for A-(a)
    { fs0F_BRK, 1, 8 },     // ...1.0.. :          : Push PC and Flags, PC = [$FFDE]

    { fs10_BPL, 2, 2 },     // ........ : r        : PC+=r  if N == 0
    { fs11_TCALL, 1, 8 },	// ........ : 1        : CALL [$FFDC]
    { fs12_CLR1, 2, 4 },	// ........ : d.0      : d.0 = 0
    { fs13_BBC, 3, 5 },     // ........ : d.0, r   : PC+=r  if d.0 == 0
    { fs14_OR, 2, 4 },      // N.....Z. : A, d+X   : A = A | (d+X)
    { fs15_OR, 3, 5 },      // N.....Z. : A, !a+X  : A = A | (a+X)
    { fs16_OR, 3, 5 },      // N.....Z. : A, !a+Y  : A = A | (a+Y)
    { fs17_OR, 2, 6 },      // N.....Z. : A, [d]+Y : A = A | ([d]+Y)
    { fs18_OR, 3, 5 },      // N.....Z. : d, #i    : (d) = (d) | i
    { fs19_OR, 1, 5 },      // N.....Z. : (X), (Y) : (X) = (X) | (Y)
    { fs1A_DECW, 2, 6 },    // N.....Z. : d        : Word (d)--
    { fs1B_ASL, 2, 5 },     // N.....ZC : d+X      : Left shift (d+X) as above
    { fs1C_ASL, 1, 2 },     // N.....ZC : A        : Left shift A: high->C, 0->low
    { fs1D_DEC, 1, 2 },     // N.....Z. : X        : X--
    { fs1E_CMP, 3, 4 },     // N.....ZC : X, !a    : X - (a)
    { fs1F_JMP, 3, 6 },     // ........ : [!a+X]   : PC = [a+X]

    { fs20_CLRP, 1, 2 },    // ..0..... :          : P = 0
    { fs21_TCALL, 1, 8 },   // ........ : 2        : CALL [$FFDA]
    { fs22_SET1, 2, 4 },    // ........ : d.1      : d.1 = 1
    { fs23_BBS, 3, 5 },     // ........ : d.1, r   : PC+=r  if d.1 == 1
    { fs24_AND, 2, 3 },     // N.....Z. : A, d     : A = A & (d)
    { fs25_AND, 3, 4 },     // N.....Z. : A, !a    : A = A & (a)
    { fs26_AND, 1, 3 },     // N.....Z. : A, (X)   : A = A & (X)
    { fs27_AND, 2, 6 },     // N.....Z. : A, [d+X] : A = A & ([d+X])
    { fs28_AND, 2, 2 },     // N.....Z. : A, #i    : A = A & i
    { fs29_AND, 3, 6 },     // N.....Z. : dd, ds   : (dd) = (dd) & (ds)
    { fs2A_OR1, 3, 5 },     // .......C : C, /m.b  : C = C | ~(m.b)
    { fs2B_ROL, 2, 4 },     // N.....ZC : d        : Left shift (d) as above
    { fs2C_ROL, 3, 5 },     // N.....ZC : !a       : Left shift (a) as above
    { fs2D_PUSH, 1, 4 },    // ........ : A        : (SP--) = A
    { fs2E_CBNE, 3, 5 },    // ........ : d, r     : CMP A, (d) then BNE
    { fs2F_BRA, 2, 4 },     // ........ : r        : PC+=r

    { fs30_BMI, 2, 2 },     // ........ : r        : PC+=r  if N == 1
    { fs31_TCALL, 1, 8 },   // ........ : 3        : CALL [$FFD8]
    { fs32_CLR1, 2, 4 },    // ........ : d.1      : d.1 = 0
    { fs33_BBC, 3, 5 },     // ........ : d.1, r   : PC+=r  if d.1 == 0
    { fs34_AND, 2, 4 },     // N.....Z. : A, d+X   : A = A & (d+X)
    { fs35_AND, 3, 5 },     // N.....Z. : A, !a+X  : A = A & (a+X)
    { fs36_AND, 3, 5 },     // N.....Z. : A, !a+Y  : A = A & (a+Y)
    { fs37_AND, 2, 6 },     // N.....Z. : A, [d]+Y : A = A & ([d]+Y)
    { fs38_AND, 3, 5 },     // N.....Z. : d, #i    : (d) = (d) & i
    { fs39_AND, 1, 5 },     // N.....Z. : (X), (Y) : (X) = (X) & (Y)
    { fs3A_INCW, 2, 6 },    // N.....Z. : d        : Word (d)++
    { fs3B_ROL, 2, 5 },     // N.....ZC : d+X      : Left shift (d+X) as above
    { fs3C_ROL, 1, 2 },     // N.....ZC : A        : Left shift A: low=C, C=high
    { fs3D_INC, 1, 2 },     // N.....Z. : X        : X++
    { fs3E_CMP, 2, 3 },     // N.....ZC : X, d     : X - (d)
    { fs3F_CALL, 3, 8 },    // ........ : !a       : (SP--)=PCh, (SP--)=PCl, PC=a

    { fs40_SETP, 1, 2 },    // ..1..... :  : P = 1
    { fs41_TCALL, 1, 8 },   // ........ : 4        : CALL [$FFD6]
    { fs42_SET1, 2, 4 },    // ........ : d.2      : d.2 = 1
    { fs43_BBS, 3, 5 },     // ........ : d.2, r   : PC+=r  if d.2 == 1
    { fs44_EOR, 2, 3 },     // N.....Z. : A, d     : A = A EOR (d)
    { fs45_EOR, 3, 4 },     // N.....Z. : A, !a    : A = A EOR (a)
    { fs46_EOR, 1, 3 },     // N.....Z. : A, (X)   : A = A EOR (X)
    { fs47_EOR, 2, 6 },     // N.....Z. : A, [d+X] : A = A EOR ([d+X])
    { fs48_EOR, 2, 2 },     // N.....Z. : A, #i    : A = A EOR i
    { fs49_EOR, 3, 6 },     // N.....Z. : dd, ds   : (dd) = (dd) EOR (ds)
    { fs4A_AND1, 3, 4 },    // .......C : C, m.b   : C = C & (m.b)
    { fs4B_LSR, 2, 4 },     // N.....ZC : d        : Right shift (d) as above
    { fs4C_LSR, 3, 5 },     // N.....ZC : !a       : Right shift (a) as above
    { fs4D_PUSH, 1, 4 },    // ........ : X        : (SP--) = X
    { fs4E_TCLR1, 3, 6 },   // N.....Z. : !a       : (a) = (a)&~A, ZN as for A-(a)
    { fs4F_PCALL, 2, 6 },   // ........ : u        : CALL $FF00+u

    { fs50_BVC, 2, 2 },     // ........ : r        : PC+=r  if V == 0
    { fs51_TCALL, 1, 8 },   // ........ : 5        : CALL [$FFD4]
    { fs52_CLR1, 2, 4 },    // ........ : d.2      : d.2 = 0
    { fs53_BBC, 3, 5 },     // ........ : d.2, r   : PC+=r  if d.2 == 0
    { fs54_EOR, 2, 4 },     // N.....Z. : A, d+X   : A = A EOR (d+X)
    { fs55_EOR, 3, 5 },     // N.....Z. : A, !a+X  : A = A EOR (a+X)
    { fs56_EOR, 3, 5 },     // N.....Z. : A, !a+Y  : A = A EOR (a+Y)
    { fs57_EOR, 2, 6 },     // N.....Z. : A, [d]+Y : A = A EOR ([d]+Y)
    { fs58_EOR, 3, 5 },     // N.....Z. : d, #i    : (d) = (d) EOR i
    { fs59_EOR, 1, 5 },     // N.....Z. : (X), (Y) : (X) = (X) EOR (Y)
    { fs5A_CMPW, 2, 4 },    // N.....ZC : YA, d    : YA - (d)
    { fs5B_LSR, 2, 5 },     // N.....ZC : d+X      : Right shift (d+X) as above
    { fs5C_LSR, 1, 2 },     // N.....ZC : A        : Right shift A: 0->high, low->C
    { fs5D_MOV, 1, 2 },     // N.....Z. : X, A     : X = A
    { fs5E_CMP, 3, 4 },     // N.....ZC : Y, !a    : Y - (a)
    { fs5F_JMP, 3, 3 },     // ........ : !a       : PC = a

    { fs60_CLRC, 1, 2 },    // .......0 :  : C = 0
    { fs61_TCALL, 1, 8 },   // ........ : 6        : CALL [$FFD2]
    { fs62_SET1, 2, 4 },    // ........ : d.3      : d.3 = 1
    { fs63_BBS, 3, 5 },     // ........ : d.3, r   : PC+=r  if d.3 == 1
    { fs64_CMP, 2, 3 },     // N.....ZC : A, d     : A - (d)
    { fs65_CMP, 3, 4 },     // N.....ZC : A, !a    : A - (a)
    { fs66_CMP, 1, 3 },     // N.....ZC : A, (X)   : A - (X)
    { fs67_CMP, 2, 6 },     // N.....ZC : A, [d+X] : A - ([d+X])
    { fs68_CMP, 2, 2 },     // N.....ZC : A, #i    : A - i
    { fs69_CMP, 3, 6 },     // N.....ZC : dd, ds   : (dd) - (ds)
    { fs6A_AND1, 3, 4 },	// .......C : C, /m.b  : C = C & ~(m.b)
    { fs6B_ROR, 2, 4 },     // N.....ZC : d        : Right shift (d) as above
    { fs6C_ROR, 3, 5 },     // N.....ZC : !a       : Right shift (a) as above
    { fs6D_PUSH, 1, 4 },    // ........ : Y        : (SP--) = Y
    { fs6E_DBNZ, 3, 5 },    // ........ : d, r     : (d)-- then JNZ
    { fs6F_RET, 1, 5 },     // ........ :          : Po      PC

    { fs70_BVS, 2, 2 },     // ........ : r        : PC+=r  if V == 1
    { fs71_TCALL, 1, 8 },   // ........ : 7        : CALL [$FFD0]
    { fs72_CLR1, 2, 4 },    // ........ : d.3      : d.3 = 0
    { fs73_BBC, 3, 5 },     // ........ : d.3, r   : PC+=r  if d.3 == 0
    { fs74_CMP, 2, 4 },     // N.....ZC : A, d+X   : A - (d+X)
    { fs75_CMP, 3, 5 },     // N.....ZC : A, !a+X  : A - (a+X)
    { fs76_CMP, 3, 5 },     // N.....ZC : A, !a+Y  : A - (a+Y)
    { fs77_CMP, 2, 6 },     // N.....ZC : A, [d]+Y : A - ([d]+Y)
    { fs78_CMP, 3, 5 },     // N.....ZC : d, #i    : (d) - i
    { fs79_CMP, 1, 5 },     // N.....ZC : (X), (Y) : (X) - (Y)
    { fs7A_ADDW, 2, 5 },	// NV..H.ZC : YA, d    : YA  = YA + (d), H on high byte
    { fs7B_ROR, 2, 5 },     // N.....ZC : d+X      : Right shift (d+X) as above
    { fs7C_ROR, 1, 2 },     // N.....ZC : A        : Right shift A: high=C, C=low
    { fs7D_MOV, 1, 2 },     // N.....Z. : A, X     : A = X
    { fs7E_CMP, 2, 3 },     // N.....ZC : Y, d     : Y - (d)
    { fs7F_RET1, 1, 6 },    // NVPBHIZC :          : Pop Flags, PC

    { fs80_SETC, 1, 2 },    // .......1 :          : C = 1
    { fs81_TCALL, 1, 8 },   // ........ : 8        : CALL [$FFCE]
    { fs82_SET1, 2, 4 },    // ........ : d.4      : d.4 = 1
    { fs83_BBS, 3, 5 },     // ........ : d.4, r   : PC+=r  if d.4 == 1
    { fs84_ADC, 2, 3 },     // NV..H.ZC : A, d     : A = A+(d)+C
    { fs85_ADC, 3, 4 },     // NV..H.ZC : A, !a    : A = A+(a)+C
    { fs86_ADC, 1, 3 },     // NV..H.ZC : A, (X)   : A = A+(X)+C
    { fs87_ADC, 2, 6 },     // NV..H.ZC : A, [d+X] : A = A+([d+X])+C
    { fs88_ADC, 2, 2 },     // NV..H.ZC : A, #i    : A = A+i+C
    { fs89_ADC, 3, 6 },     // NV..H.ZC : dd, ds   : (dd) = (dd)+(d)+C
    { fs8A_EOR1, 3, 5 },    // .......C : C, m.b   : C = C EOR (m.b)
    { fs8B_DEC, 2, 4 },     // N.....Z. : d        : (d)--
    { fs8C_DEC, 3, 5 },     // N.....Z. : !a       : (a)--
    { fs8D_MOV, 2, 2 },     // N.....Z. : Y, #i    : Y = i
    { fs8E_POP, 1, 4 },     // NVPBHIZC : PSW      : Flags = (++SP)
    { fs8F_MOV, 3, 5 },     // ........ : d, #i    : (d) = i        (read)

    { fs90_BCC, 2, 2 },     // ........ : r        : PC+=r  if C == 0
    { fs91_TCALL, 1, 8 },   // ........ : 9        : CALL [$FFCC]
    { fs92_CLR1, 2, 4 },    // ........ : d.4      : d.4 = 0
    { fs93_BBC, 3, 5 },     // ........ : d.4, r   : PC+=r  if d.4 == 0
    { fs94_ADC, 2, 4 },     // NV..H.ZC : A, d+X   : A = A+(d+X)+C
    { fs95_ADC, 3, 5 },     // NV..H.ZC : A, !a+X  : A = A+(a+X)+C
    { fs96_ADC, 3, 5 },     // NV..H.ZC : A, !a+Y  : A = A+(a+Y)+C
    { fs97_ADC, 2, 6 },     // NV..H.ZC : A, [d]+Y : A = A+([d]+Y)+C
    { fs98_ADC, 3, 5 },     // NV..H.ZC : d, #i    : (d) = (d)+i+C
    { fs99_ADC, 1, 5 },     // NV..H.ZC : (X), (Y) : (X) = (X)+(Y)+C
    { fs9A_SUBW, 2, 5 },    // NV..H.ZC : YA, d    : YA  = YA - (d), H on high byte
    { fs9B_DEC, 2, 5 },     // N.....Z. : d+X      : (d+X)--
    { fs9C_DEC, 1, 2 },     // N.....Z. : A        : A--
    { fs9D_MOV, 1, 2 },     // N.....Z. : X, SP    : X = SP
    { fs9E_DIV, 1, 1 },     // NV..H.Z. : YA, X    : A=YA/X, Y=mod(YA,X)
    { fs9F_XCN, 1, 5 },     // N.....Z. : A        : A = (A>>4) | (A<<4)

    { fsA0_EI, 1, 3 },      // .....1.. :          : I = 1
    { fsA1_TCALL, 1, 8 },   // ........ : 10       : CALL [$FFCA]
    { fsA2_SET1, 2, 4 },    // ........ : d.5      : d.5 = 1
    { fsA3_BBS, 3, 5 },     // ........ : d.5, r   : PC+=r  if d.5 == 1
    { fsA4_SBC, 2, 3 },     // NV..H.ZC : A, d     : A = A-(d)-!C
    { fsA5_SBC, 3, 4 },     // NV..H.ZC : A, !a    : A = A-(a)-!C
    { fsA6_SBC, 1, 3 },     // NV..H.ZC : A, (X)   : A = A-(X)-!C
    { fsA7_SBC, 2, 6 },     // NV..H.ZC : A, [d+X] : A = A-([d+X])-!C
    { fsA8_SBC, 2, 2 },     // NV..H.ZC : A, #i    : A = A-i-!C
    { fsA9_SBC, 3, 6 },     // NV..H.ZC : dd, ds   : (dd) = (dd)-(ds)-!C
    { fsAA_MOV1, 3, 4 },    // .......C : C, m.b   : C = (m.b)
    { fsAB_INC, 2, 4 },     // N.....Z. : d        : (d)++
    { fsAC_INC, 3, 5 },     // N.....Z. : !a       : (a)++
    { fsAD_CMP, 2, 2 },     // N.....ZC : Y, #i    : Y - i
    { fsAE_POP, 1, 4 },     // ........ : A        : A = (++SP)
    { fsAF_MOV, 1, 4 },     // ........ : (X)+, A  : (X++) = A      (no read)

    { fsB0_BCS, 2, 2 },     // ........ : r        : PC+=r  if C == 1
    { fsB1_TCALL, 1, 8 },   // ........ : 11       : CALL [$FFC8]
    { fsB2_CLR1, 2, 4 },    // ........ : d.5      : d.5 = 0
    { fsB3_BBC, 3, 5 },     // ........ : d.5, r   : PC+=r  if d.5 == 0
    { fsB4_SBC, 2, 4 },     // NV..H.ZC : A, d+X   : A = A-(d+X)-!C
    { fsB5_SBC, 3, 5 },     // NV..H.ZC : A, !a+X  : A = A-(a+X)-!C
    { fsB6_SBC, 3, 5 },     // NV..H.ZC : A, !a+Y  : A = A-(a+Y)-!C
    { fsB7_SBC, 2, 6 },     // NV..H.ZC : A, [d]+Y : A = A-([d]+Y)-!C
    { fsB8_SBC, 3, 5 },     // NV..H.ZC : d, #i    : (d) = (d)-i-!C
    { fsB9_SBC, 1, 5 },     // NV..H.ZC : (X), (Y) : (X) = (X)-(Y)-!C
    { fsBA_MOVW, 2, 5 },    // N.....Z. : YA, d    : YA = word (d)
    { fsBB_INC, 2, 5 },     // N.....Z. : d+X      : (d+X)++
    { fsBC_INC, 1, 2 },     // N.....Z. : A        : A++
    { fsBD_MOV, 1, 2 },     // ........ : SP, X    : SP = X
    { fsBE_DAS, 1, 3 },     // N.....ZC : A        : decimal adjust for subtraction
    { fsBF_MOV, 1, 4 },     // N.....Z. : A, (X)+  : A = (X++)

    { fsC0_DI, 1, 3 },      // .....0.. :          : I = 0
    { fsC1_TCALL, 1, 8 },   // ........ : 12       : CALL [$FFC6]
    { fsC2_SET1, 2, 4 },    // ........ : d.6      : d.6 = 1
    { fsC3_BBS, 3, 5 },     // ........ : d.6, r   : PC+=r  if d.6 == 1
    { fsC4_MOV, 2, 4 },     // ........ : d, A     : (d) = A        (read)
    { fsC5_MOV, 3, 5 },     // ........ : !a, A    : (a) = A        (read)
    { fsC6_MOV, 1, 4 },     // ........ : (X), A   : (X) = A        (read)
    { fsC7_MOV, 2, 7 },     // ........ : [d+X], A : ([d+X]) = A    (read)
    { fsC8_CMP, 2, 2 },     // N.....ZC : X, #i    : X - i
    { fsC9_MOV, 3, 5 },     // ........ : !a, X    : (a) = X        (read)
    { fsCA_MOV1, 3, 6 },    // ........ : m.b, C   : (m.b) = C
    { fsCB_MOV, 2, 4 },     // ........ : d, Y     : (d) = Y        (read)
    { fsCC_MOV, 3, 5 },     // ........ : !a, Y    : (a) = Y        (read)
    { fsCD_MOV, 2, 2 },     // N.....Z. : X, #i    : X = i
    { fsCE_POP, 1, 4 },     // ........ : X        : X = (++SP)
    { fsCF_MUL, 1, 9 },     // N.....Z. : YA       : YA = Y * A, NZ on Y only

    { fsD0_BNE, 2, 2 },     // ........ : r        : PC+=r  if Z == 0
    { fsD1_TCALL, 1, 8 },   // ........ : 13       : CALL [$FFC4]
    { fsD2_CLR1, 2, 4 },    // ........ : d.6      : d.6 = 0
    { fsD3_BBC, 3, 5 },     // ........ : d.6, r   : PC+=r  if d.6 == 0
    { fsD4_MOV, 2, 5 },     // ........ : d+X, A   : (d+X) = A      (read)
    { fsD5_MOV, 3, 6 },     // ........ : !a+X, A  : (a+X) = A      (read)
    { fsD6_MOV, 3, 6 },     // ........ : !a+Y, A  : (a+Y) = A      (read)
    { fsD7_MOV, 2, 7 },     // ........ : [d]+Y, A : ([d]+Y) = A    (read)
    { fsD8_MOV, 2, 4 },     // ........ : d, X     : (d) = X        (read)
    { fsD9_MOV, 2, 5 },     // ........ : d+Y, X   : (d+Y) = X      (read)
    { fsDA_MOVW, 2, 5 },    // ........ : d, YA    : word (d) = YA  (read low only)
    { fsDB_MOV, 2, 5 },     // ........ : d+X, Y   : (d+X) = Y      (read)
    { fsDC_DEC, 1, 2 },     // N.....Z. : Y        : Y--
    { fsDD_MOV, 1, 2 },     // N.....Z. : A, Y     : A = Y
    { fsDE_CBNE, 3, 6 },    // ........ : d+X, r   : CMP A, (d+X) then BNE
    { fsDF_DAA, 1, 3 },     // N.....ZC : A        : decimal adjust for addition

    { fsE0_CLRV, 1, 2 },    // .0..0... :          : V = 0, H = 0
    { fsE1_TCALL, 1, 8 },   // ........ : 14       : CALL [$FFC2]
    { fsE2_SET1, 2, 4 },    // ........ : d.7      : d.7 = 1
    { fsE3_BBS, 3, 5 },     // ........ : d.7, r   : PC+=r  if d.7 == 1
    { fsE4_MOV, 2, 3 },     // N.....Z. : A, d     : A = (d)
    { fsE5_MOV, 3, 4 },     // N.....Z. : A, !a    : A = (a)
    { fsE6_MOV, 1, 3 },     // N.....Z. : A, (X)   : A = (X)
    { fsE7_MOV, 2, 6 },     // N.....Z. : A, [d+X] : A = ([d+X])
    { fsE8_MOV, 2, 2 },     // N.....Z. : A, #i    : A = i
    { fsE9_MOV, 3, 4 },     // N.....Z. : X, !a    : X = (a)
    { fsEA_NOT1, 3, 5 },    // ........ : m.b      : m.b = ~m.b
    { fsEB_MOV, 2, 3 },     // N.....Z. : Y, d     : Y = (d)
    { fsEC_MOV, 3, 4 },     // N.....Z. : Y, !a    : Y = (a)
    { fsED_NOTC, 1, 3 },    // .......C :          : C = !C
    { fsEE_POP, 1, 4 },     // ........ : Y        : Y = (++SP)
    { fsEF_SLEEP, 1, 0 },   // ........ :          : Halts the processor

    { fsF0_BEQ, 2, 2 },     // ........ : r        : PC+=r  if Z == 1
    { fsF1_TCALL, 1, 8 },   // ........ : 15       : CALL [$FFC0]
    { fsF2_CLR1, 2, 4 },    // ........ : d.7      : d.7 = 0
    { fsF3_BBC, 3, 5 },     // ........ : d.7, r   : PC+=r  if d.7 == 0
    { fsF4_MOV, 2, 4 },     // N.....Z. : A, d+X   : A = (d+X)
    { fsF5_MOV, 3, 5 },     // N.....Z. : A, !a+X  : A = (a+X)
    { fsF6_MOV, 3, 5 },     // N.....Z. : A, !a+Y  : A = (a+Y)
    { fsF7_MOV, 2, 6 },     // N.....Z. : A, [d]+Y : A = ([d]+Y)
    { fsF8_MOV, 2, 3 },     // N.....Z. : X, d     : X = (d)
    { fsF9_MOV, 2, 4 },     // N.....Z. : X, d+Y   : X = (d+Y)
    { fsFA_MOV, 3, 5 },     // ........ : dd, ds   : (dd) = (ds)    (no read)
    { fsFB_MOV, 2, 4 },     // N.....Z. : Y, d+X   : Y = (d+X)
    { fsFC_INC, 1, 2 },     // N.....Z. : Y        : Y++
    { fsFD_MOV, 1, 2 },     // N.....Z. : Y, A     : Y = A
    { fsFE_DBNZ, 2, 4 },    // ........ : Y, r     : Y-- then JNZ
    { fsFF_STOP, 1, 0 }     // ........ :          : Halts the processor 
};

#pragma endregion