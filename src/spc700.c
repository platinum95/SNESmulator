
#include "spc700.h"

#include <stdbool.h>
#include <stdint.h>
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
    uint8_t timer0;
    uint8_t timer1;
    uint8_t timer2;
    uint8_t counter0;
    uint8_t counter1;
    uint8_t counter2;
} Registers;

uint8_t *spc_memory_map( uint16_t addr );

/* Memory and registers */
static uint8_t spc_memory[ 0xFFFF + 0x01 ];
static uint16_t program_counter, next_program_counter, curr_program_counter;
static uint8_t A, X, Y, SP, PSW;
static Registers* registers = (Registers*)( spc_memory + 0X00F0 );
static uint8_t opCycles;

static uint8_t IPL_ROM[ 64 ] = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

/* Initialise (power on) */
void spc700_initialise() {
    registers->port0 = 0xAA;
    registers->port1 = 0xBB;
    SP = 0xEF;
    program_counter = 0xFFC0;
    memcpy( &spc_memory[ 0xFFC0 ], IPL_ROM, sizeof( IPL_ROM ) );
}

/* Execute next instruction, update PC and cycle counter etc */
void spc700_execute_next_instruction() {
    curr_program_counter = program_counter;
    SPC700InstructionEntry *entry = &instructions[ *spc_memory_map( program_counter++ ) ];
    opCycles = entry->opCycles;
    next_program_counter = curr_program_counter + entry->opLength;
    entry->instruction();

    // Operation may mutate next_program_counter if it branches/jumps
    program_counter = next_program_counter;
}

/* Access the 4 visible bytes from the CPU */
uint8_t *access_spc_snes_mapped( uint16_t addr ) {
    addr = addr - 0x2140 + 0x00f4;
    
    if ( addr < 0x00f4 || addr > 0x00f7 ) {
        return NULL;
    }
    return spc_memory_map( addr );
}

/* Status register flags */
#define SPC_CARRY_FLAG  0x1
#define SPC_ZERO_FLAG 0x2
#define SPC_INTERRUPT_FLAG 0x04
#define SPC_HALF_CARRY_FLAG 0x08
#define SPC_BREAK_FLAG 0x10
#define SPC_P_FLAG 0x20
#define SPC_OVERFLOW_FLAG 0x40
#define SPC_NEGATIVE_FLAG 0x80

/* 16 bit "register" from A and Y registers */
static uint16_t getYA() {
    return ( ( (uint16_t) Y ) << 8 ) | A;
}

static void storeYA( uint16_t YA ) {
    Y = ( YA >> 8 ) & 0x00FF;
    A = YA & 0x00FF;
}

// TODO - consider renaming to getWord
uint16_t getWord(uint8_t* loc) {
    uint16_t out = loc[1];
    out = out << 8;
    out = out | (0x00FF & loc[0]);
    return out;
}

// TODO - consider renaming to setWord
void setWord( uint8_t* loc, uint16_t val ) {
    loc[ 1 ] = (uint8_t)( ( val >> 8 ) & 0x00FF );
    loc[ 0 ] = (uint8_t)( val & 0x00FF );
}

/* 16 bit stack pointer = 0x0100 | SP_register 
static uint16_t get_stack_pointer() {
    uint16_t toRet = SP;
    return toRet | 0x01;
}
*/

uint8_t *spc_memory_map( uint16_t addr ) {
    if ( addr > 0x0000 && addr <= 0x00EF ) {
        return &spc_memory[ addr ];
    }
    else if ( addr <= 0x00FF ) {
        // Registers
        return &spc_memory[ addr ];
    }
    else if ( addr <= 0x01FF ) {
        // Page 1 and Memory
        return &spc_memory[ addr ];
    }
    else { // if ( addr <= 0xFFFF )
        // Memory (Read/ReadWrite)/IPL Rom
        // TODO - adjust based on X reg
        return &spc_memory[ addr ];
    }
}

uint8_t *accessPageAddr( uint8_t addr ) {
    return spc_memory_map( ( ( PSW & SPC_P_FLAG ) ? 0x0100 : 0x0000 ) + addr );
}

#pragma region SPC_ADDRESSING_MODES
static uint8_t *immediate() {
    return spc_memory_map( program_counter++ );
}
static uint8_t immediate_8() {
    return *immediate();
}
static uint8_t *indirect_X() {
    return accessPageAddr( X );
}

static uint8_t *indirect_Y() {
    return accessPageAddr( Y );
}

/* // TODO - confirm that this isn't used
static uint8_t indirect_Y_AI_8() {
    uint8_t toRet = *indirect_Y();
    ++Y;
    return toRet;
}
static uint8_t indirect_X_AI_8() {
    uint8_t toRet = *indirect_X();
    ++X;
    return toRet;
}*/

static uint8_t *direct( uint8_t offset ) {
    return accessPageAddr( immediate_8() + offset );
}
static uint8_t direct_8( uint8_t offset ) {
    return *direct( offset );
}
static uint16_t direct_16( uint8_t offset ) {
    return getWord( direct( offset ) );
}

/* // TODO - Confirm that this isn't used
static uint16_t direct_indexed_X_16() {
    return getWord( direct( X ) );
}

static uint8_t *direct_indexed_X_indirect() {
    return spc_memory_map( direct_indexed_X_16() );
}

static uint8_t *x_indexed_indirect() {
    return spc_memory_map( direct_16( X ) );
}
static uint8_t *indirect_y_indexed() {
    return spc_memory_map( direct_16( 0 ) + Y );
}
*/

static uint16_t absolute_addr() {
    uint16_t word = getWord( spc_memory_map( program_counter ) );
    program_counter += 2;
    return word;
}
static uint8_t *absolute( uint8_t offset ) {
    return spc_memory_map( absolute_addr() + offset );
}

static uint8_t absolute_membit( uint8_t **mem ) {
    uint16_t operand = absolute_addr();
    uint8_t bitLoc = ( operand >> 13 ) & 0x0007;
    uint16_t addr = operand & 0x1FFF;
    *mem = spc_memory_map( addr );
    return bitLoc;
}

// DIRECT OPS START
#define DIRECT_D_OP( op ) \
    op( direct( 0 ) );

#define DIRECT_A_D_OP( op ) \
    op( &A, direct( 0 ) );

#define DIRECT_D_A_OP( op ) \
    op( direct( 0 ), &A );

#define DIRECT_X_D_OP( op ) \
    op( &X, direct( 0 ) );

#define DIRECT_D_X_OP( op ) \
    op( direct( 0 ), &X );

#define DIRECT_Y_D_OP( op ) \
    op( &Y, direct( 0 ) ); 

#define DIRECT_D_Y_OP( op ) \
    op( direct( 0 ), &Y );

#define DIRECT_YA_D_OP( op ) \
    uint16_t YA = getYA(); \
    op( &YA, direct( 0 ) ); \
    storeYA( YA );
// DIRECT OPS END

// X-INDEXED DIRECT PAGE OPS START
#define X_INDEXED_DIRECT_PAGE_DX_OP( op ) \
    op( direct( X ) );

#define X_INDEXED_DIRECT_PAGE_Y_DX_OP( op ) \
    op( &Y, direct( X ) );

#define X_INDEXED_DIRECT_PAGE_DX_Y_OP( op ) \
    op( direct( X ), &Y );

#define X_INDEXED_DIRECT_PAGE_A_DX_OP( op ) \
    op( &A, direct( X ) );

#define X_INDEXED_DIRECT_PAGE_DX_A_OP( op ) \
    op( direct( X ), &A );

// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define X_INDEXED_DIRECT_PAGE_DX_R_OP( op ) \
    uint8_t nearLabel = immediate_8(); \
    uint8_t *dp_X = direct( X ); \
    op( dp_X, &nearLabel );
// X-INDEXED DIRECT PAGE OPS END

// Y-INDEXED DIRECT PAGE OPS START
#define Y_INDEXED_DIRECT_PAGE_X_DY_OP( op ) \
    op( &X, direct( Y ) );

#define Y_INDEXED_DIRECT_PAGE_DY_X_OP( op ) \
    op( direct( Y ), &X );
// Y-INDEXED DIRECT PAGE OPS END

// INDIRECT OPS START
#define INDIRECT_A_X_OP( op ) \
    op( &A, indirect_X() );

#define INDIRECT_X_A_OP( op ) \
    op( indirect_X(), &A );
// INDIRECT OPS END

// INDIRECT AUTO INC OPS START
#define INDIRECT_AUTO_INC_X_A_OP( op ) \
    op( indirect_X(), &A ); \
    ++X; \

#define INDIRECT_AUTO_INC_A_X_OP( op ) \
    op( &A, indirect_X() ); \
    ++X; \
// INDIRECT AUTO INC OPS END

// DIRECT PAGE TO DIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define DIRECT_PAGE_DIRECT_PAGE_OP( op ) \
    uint8_t *ds = direct( 0 ); \
    uint8_t *dd = direct( 0 ); \
    op( dd, ds ); \
// DIRECT PAGE TO DIRECT PAGE OPS END


// INDIRECT PAGE TO INDIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define INDIRECT_PAGE_INDIRECT_PAGE_OP( op ) \
    uint8_t* iY = indirect_Y(); \
    uint8_t* iX = indirect_X(); \
    op( iX, iY ); \
// INDIRECT PAGE TO INDIRECT PAGE OPS END

// IMMEDIATE TO DIRECT PAGE OPS START
// TODO - param ordering verification - Should be correct if little-endian
#define IMMEDIATE_TO_DIRECT_PAGE_OP( op ) \
    uint8_t *im = immediate(); \
    uint8_t *dp = direct( 0 ); \
    op( dp, im );
// IMMEDIATE TO DIRECT PAGE OPS END

// DIRECT PAGE BIT OPS START
#define DIRECT_PAGE_BIT_OP( op, bit ) \
    op( direct( 0 ), 0x01 << bit );
// DIRECT PAGE BIT OPS END

// DIRECT PAGE BIT RELATIVE OPS START
// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define DIRECT_PAGE_BIT_RELATIVE_OP( op, bit ) \
    uint8_t d = direct_8( 0 ); \
    uint8_t *r = immediate(); \
    op( d & ( 0x01 << bit ), r ); 
// DIRECT PAGE BIT RELATIVE OPS END

// ABSOLUTE BOOLEAN BIT OPS START
// These take 1 or 2 bool inputs, and return a bool output value
#define ABSOLUTE_BOOLEAN_BIT_BASE( op, outLoc, outMask, inVal ) \
    (*outLoc) = ( (*outLoc) & ~outMask ) | ( op( inVal, *outLoc & outMask ) ? outMask : 0x00 );

#define ABSOLUTE_BOOLEAN_BIT_MB_OP( op ) \
    uint8_t *loc; \
    uint8_t bit = absolute_membit( &loc ); \
    uint8_t bMask = 0x01 << bit; \
    *loc = ( *loc & ~bMask ) | ( op( *loc & bMask ) ? bMask : 0x00 );
    
#define ABSOLUTE_BOOLEAN_BIT_C_MB_OP( op ) \
    uint8_t *loc; \
    uint8_t bit = absolute_membit( &loc ); \
    uint8_t bMask = 0x01 << bit; \
    ABSOLUTE_BOOLEAN_BIT_BASE( op, &PSW, SPC_CARRY_FLAG, (*loc) & bMask );

#define ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( op ) \
    uint8_t *loc; \
    uint8_t bit = absolute_membit( &loc ); \
    uint8_t bMask = 0x01 << bit; \
    ABSOLUTE_BOOLEAN_BIT_BASE( op, &PSW, SPC_CARRY_FLAG, (*loc) & bMask );

#define ABSOLUTE_BOOLEAN_BIT_MB_C_OP( op ) \
    uint8_t *loc; \
    uint8_t bit = absolute_membit( &loc ); \
    uint8_t bMask = 0x01 << bit; \
    ABSOLUTE_BOOLEAN_BIT_BASE( op, loc, bMask, PSW & SPC_CARRY_FLAG );
// ABSOLUTE BOOLEAN BIT OPS END

// ABSOLUTE OPS START
#define ABSOLUTE_a_addr_OP( op ) \
    op( absolute_addr() );

#define ABSOLUTE_a_OP( op ) \
    op( absolute( 0 ) );

#define ABSOLUTE_A_a_OP( op ) \
    op( &A, absolute( 0 ) );

#define ABSOLUTE_a_A_OP( op ) \
    op( absolute( 0 ), &A );

#define ABSOLUTE_X_a_OP( op ) \
    op( &X, absolute( 0 ) );

#define ABSOLUTE_a_X_OP( op ) \
    op( absolute( 0 ), &X );

#define ABSOLUTE_Y_a_OP( op ) \
    op( &Y, absolute( 0 ) );

#define ABSOLUTE_a_Y_OP( op ) \
    op( absolute( 0 ), &Y );
// ABSOLUTE OPS END

// ABSOLUTE X-INDEXED INDIRECT OPS START
#define ABSOLUTE_X_INDEXED_INDIRECT_ADDR_OP( op )\
    op( absolute_addr() + X );
// ABSOLUTE X-INDEXED INDIRECT OPS END

// X-INDEXED ABSOLUTE OPS START
#define X_INDEXED_ABSOLUTE_A_aX_OP( op )\
    op( &A, absolute( X ) );

#define X_INDEXED_ABSOLUTE_aX_A_OP( op )\
    op( absolute( X ), &A );
// X-INDEXED ABSOLUTE OPS END

// Y-INDEXED ABSOLUTE OPS START
#define Y_INDEXED_ABSOLUTE_A_aY_OP( op )\
    op( &A, absolute( Y ) );

#define Y_INDEXED_ABSOLUTE_aY_A_OP( op )\
    op( absolute( Y ), &A );
// Y-INDEXED ABSOLUTE OPS END

// X-INDEXED INDIRECT OPS START
#define X_INDEXED_INDIRECT_A_dX_OP( op )\
    op( &A, spc_memory_map( direct_16( X ) ) );

#define X_INDEXED_INDIRECT_dX_A_OP( op )\
    op( spc_memory_map( direct_16( X ) ), &A );
// X-INDEXED INDIRECT OPS END

// INDIRECT Y-INDEXED OPS START
#define INDIRECT_Y_INDEXED_A_dY_OP( op )\
    op( &A, spc_memory_map( direct_16( 0 ) + Y ) );

#define INDIRECT_Y_INDEXED_dY_A_OP( op )\
    op( spc_memory_map( direct_16( 0 ) + Y ), &A );
// INDIRECT Y-INDEXED OPS END

// RELATIVE OPS START
#define RELATIVE_OP( op ) \
    op( curr_program_counter + immediate_8() ); // TODO - verify this

// Ordering here is the exception to the little-endian operands rule.
// BBS $01.2, $05 is stored as <BBS.2> 01 02
#define RELATIVE_OP_D_R( op ) \
    uint8_t *d = direct( 0 ); \
    uint8_t *r = immediate(); \
    op( d, r );

#define RELATIVE_OP_Y_R( op ) \
    op( &Y, immediate() );
// RELATIVE OPS END

// IMMEDIATE OPS START
#define IMMEDIATE_A_I_OP( op ) \
    op( &A, immediate() );

#define IMMEDIATE_X_I_OP( op ) \
    op( &X, immediate() );

#define IMMEDIATE_Y_I_OP( op ) \
    op( &Y, immediate() );
// IMMEDIATE OPS END

// ACCUMULATOR OPS START
#define ACCUMULATOR_OP( op ) \
    op( &A );
// ACCUMULATOR OPS END

// IMPLIED OPS START
#define IMPLIED_A_OP( op ) \
    ACCUMULATOR_OP( op )

#define IMPLIED_X_OP( op ) \
    op( &X );

#define IMPLIED_Y_OP( op ) \
    op( &Y );

#define IMPLIED_PSW_OP( op ) \
    op( &PSW );

#define IMPLIED_X_A_OP( op ) \
    op( &X, &A );

#define IMPLIED_A_X_OP( op ) \
    op( &A, &X );

#define IMPLIED_Y_A_OP( op ) \
    op( &Y, &A );

#define IMPLIED_A_Y_OP( op ) \
    op( &A, &Y );

#define IMPLIED_SP_X_OP( op ) \
    op( &SP, &X );

#define IMPLIED_X_SP_OP( op ) \
    op( &X, &SP );

#define IMPLIED_YA_OP( op ) \
    uint16_t YA; \
    op( &YA ); \
    storeYA( YA );

#define IMPLIED_YA_X_OP( op ) \
    uint16_t YA; \
    op( &YA, &X ); \
    storeYA( YA );
// IMPLIED OPS END


#pragma endregion

void spc_set_PSW_register( uint8_t val, uint8_t flags ) {
    if ( val == 0 )
        PSW |= SPC_ZERO_FLAG & flags;
    else
        PSW &= ~( SPC_ZERO_FLAG & flags );

    if ( val & 0x80 )
        PSW |= SPC_NEGATIVE_FLAG & flags;
    else
        PSW &= ~( SPC_NEGATIVE_FLAG & flags );
}

#pragma region SPC_INSTRUCTIONS


#pragma region SPC_STACK
static void POP( uint8_t *O1 ) {
    *O1 = *accessPageAddr( ++SP );
}

static void PUSH( uint8_t *O1 ) {
    *accessPageAddr( SP-- ) = *O1;
}

static void fsAE_POP() {
    IMPLIED_A_OP( POP );
}
static void fs8E_POP() {
    IMPLIED_PSW_OP( POP );
}
static void fsCE_POP() {
    IMPLIED_X_OP( POP );
}
static void fsEE_POP() {
    IMPLIED_Y_OP( POP );
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

static void ADC( uint8_t *O1, uint8_t *O2 ) {
    uint8_t C = ( PSW & SPC_CARRY_FLAG ) ? 0x01 : 0x00;

    uint16_t result = ( (uint16_t)*O1 ) + ( (uint16_t)*O2 ) + ( (uint16_t)C );
    uint8_t result8 = (uint8_t)( result & 0x00FF );
    if ( (~( *O1 ^ *O2 ) & ( *O1 ^ result8 ) ) & 0x80 ) {
        PSW |= SPC_OVERFLOW_FLAG;
    }
    if ( ( ( *O1 & 0x0F ) + ( *O2 & 0x0F ) + C ) > 0x0F ) {
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
    *O1 = result8;
}

static void SBC( uint8_t *O1, uint8_t *O2 ) {
    uint8_t Bneg = ( ~( *O2 + ( PSW & SPC_CARRY_FLAG ? 0 : 1 ) ) ) + 1;
    ADC( O1, &Bneg );
}

static void fs99_ADC() {
    INDIRECT_PAGE_INDIRECT_PAGE_OP( ADC );
}
static void fs88_ADC() {
    IMMEDIATE_A_I_OP( ADC );
}
static void fs86_ADC() {
    INDIRECT_X_A_OP( ADC );
}
static void fs97_ADC() {
    INDIRECT_Y_INDEXED_A_dY_OP( ADC );
}
static void fs87_ADC() {
    X_INDEXED_INDIRECT_A_dX_OP( ADC );
}
static void fs84_ADC() {
    DIRECT_A_D_OP( ADC );
}
static void fs94_ADC() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( ADC );
}
static void fs85_ADC() {
    ABSOLUTE_A_a_OP( ADC );
}
static void fs95_ADC() {
    X_INDEXED_ABSOLUTE_A_aX_OP( ADC );
}
static void fs96_ADC() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( ADC );
}
static void fs89_ADC() {
    DIRECT_PAGE_DIRECT_PAGE_OP( ADC );
}
static void fs98_ADC() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( ADC );
}
static void fs7A_ADDW() {
    // TODO
    uint16_t YA = getYA();
    uint16_t O1 = direct_16( 0 );
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
    INDIRECT_PAGE_INDIRECT_PAGE_OP( SBC );
}
static void fsA8_SBC() {
    IMMEDIATE_A_I_OP( SBC );
}
static void fsA6_SBC() {
    INDIRECT_A_X_OP( SBC );
}
static void fsB7_SBC() {
    INDIRECT_Y_INDEXED_A_dY_OP( SBC );
}
static void fsA7_SBC() {
    X_INDEXED_INDIRECT_A_dX_OP( SBC );
}
static void fsA4_SBC() {
    DIRECT_A_D_OP( SBC );
}
static void fsB4_SBC() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( SBC );
}
static void fsA5_SBC() {
    ABSOLUTE_A_a_OP( SBC );
}
static void fsB5_SBC() {
    X_INDEXED_ABSOLUTE_A_aX_OP( SBC );
}
static void fsB6_SBC() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( SBC );
}
static void fsA9_SBC() {
    DIRECT_PAGE_DIRECT_PAGE_OP( SBC );
}
static void fsB8_SBC() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( SBC );
}
static void fs9A_SUBW() {
    // TODO
    uint32_t YA = (uint32_t) getYA();
    uint32_t O1 = (uint32_t) getWord( direct( 0 ) );

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
static void AND( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O1 & *O2;

    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 ) 
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static bool AND1( bool O1, bool O2 ) {
    return O1 && O2;
}

static void fs39_AND() {
    INDIRECT_PAGE_INDIRECT_PAGE_OP( AND );
}
static void fs28_AND() {
    IMMEDIATE_A_I_OP( AND );
}
static void fs26_AND() {
    INDIRECT_A_X_OP( AND );
}
static void fs37_AND() {
    INDIRECT_Y_INDEXED_A_dY_OP( AND );
}
static void fs27_AND() {
    X_INDEXED_INDIRECT_A_dX_OP( AND );
}
static void fs24_AND() {
    DIRECT_A_D_OP( AND );
}
static void fs34_AND() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( AND );
}
static void fs25_AND() {
    ABSOLUTE_A_a_OP( AND );
}
static void fs35_AND() {
    X_INDEXED_ABSOLUTE_A_aX_OP( AND );
}
static void fs36_AND() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( AND );
}
static void fs29_AND() {
    DIRECT_PAGE_DIRECT_PAGE_OP( AND );
}
static void fs38_AND() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( AND );
}
static void fs6A_AND1() {
    ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( AND1 );
}
static void fs4A_AND1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( AND1 );
}

#pragma endregion 

#pragma region SPC_BIT_SHIFT
static void LSR( uint8_t *O1 ) {
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( *O1 & 0x01 ) ? SPC_CARRY_FLAG : 0x00 );
    *O1 = ( *O1 >> 1 ) & ~0x7F;

    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( *O1 == 0x00 ) ? SPC_ZERO_FLAG : 0x00 );
}
static void ASL( uint8_t *O1 ) {
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( *O1 & 0x80 ) ? SPC_CARRY_FLAG : 0x00 );
    *O1 = ( *O1 << 1 ) & 0xFE;

    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( *O1 == 0x00 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static void fs1C_ASL() {
    IMPLIED_A_OP( ASL );
}
static void fs0B_ASL() {
    DIRECT_D_OP( ASL );
}
static void fs1B_ASL() {
    X_INDEXED_DIRECT_PAGE_DX_OP( ASL );
}
static void fs0C_ASL() {
    ABSOLUTE_a_OP( ASL );
}

static void fs5C_LSR() {
    IMPLIED_A_OP( LSR );
}
static void fs4B_LSR() {
    DIRECT_D_OP( LSR );
}
static void fs5B_LSR() {
    X_INDEXED_DIRECT_PAGE_DX_OP( LSR );
}
static void fs4C_LSR() {
    ABSOLUTE_a_OP( LSR );
}

static void ROL( uint8_t *O1 ) {
    uint8_t lsb = PSW & SPC_CARRY_FLAG ? 0x01 : 0x00;
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( *O1 & 0x80 ) ? SPC_CARRY_FLAG : 0x00 );
    *O1 = ( ( *O1 << 1 ) & 0xFE ) | lsb;
}

static void ROR( uint8_t *O1 ) {
    uint8_t msb = PSW & SPC_CARRY_FLAG ? 0x80 : 0x00;
    PSW = ( PSW & ~SPC_CARRY_FLAG ) | ( ( *O1 & 0x01 ) ? SPC_CARRY_FLAG : 0x00 );
    *O1 = ( ( *O1 >> 1 ) & 0x7F ) | msb;
}

static void fs3C_ROL() {
    IMPLIED_A_OP( ROL );
}
static void fs2B_ROL() {
    DIRECT_D_OP( ROL );
}
static void fs3B_ROL() {
    X_INDEXED_DIRECT_PAGE_DX_OP( ROL );
}
static void fs2C_ROL() {
    ABSOLUTE_a_OP( ROL );
}

static void fs7C_ROR() {
    IMPLIED_A_OP( ROR );
}
static void fs6B_ROR() {
    DIRECT_D_OP( ROR );
}
static void fs7B_ROR() {
    X_INDEXED_DIRECT_PAGE_DX_OP( ROR );
}
static void fs6C_ROR() {
    ABSOLUTE_a_OP( ROR );
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
    BranchOnCondition( condition, (int8_t) immediate_8() );
}

static void BBC( bool O1, uint8_t* r ) {
    BranchOnCondition( O1, (int8_t) *r );
}

static void BBS( bool O1, uint8_t* r ) {
    BranchOnCondition( O1, (int8_t) *r );
}

static void JMP( uint16_t addr ) {
    next_program_counter = addr;
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
static void fs0F_BRK() {
    // TODO
    PSW |= SPC_BREAK_FLAG;
    uint8_t r = ( next_program_counter >> 8 ) & 0x00FF;
    PUSH( &r );
    r = next_program_counter & 0x00FF;
    PUSH( &r );
    PUSH( &PSW );

    PSW &= ~SPC_INTERRUPT_FLAG;
    JMP( 0xFFDE );
}

static void fs1F_JMP() {
    // TODO - verify that its 'pc = im_16 + X', not 'pc = mem[ im_16 + X ]'
    ABSOLUTE_X_INDEXED_INDIRECT_ADDR_OP( JMP );
    
}
static void fs5F_JMP() {
    ABSOLUTE_a_addr_OP( JMP );
}

#pragma endregion 

#pragma region SPC_CLR
static void CLR( uint8_t* O1, uint8_t mask ) {
    *O1 &= ~mask;
}
static void fs12_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 0 );
}
static void fs32_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 1 );
}
static void fs52_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 2 );
}
static void fs72_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 3 );
}
static void fs92_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 4 );
}
static void fsB2_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 5 );
}
static void fsD2_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 6 );
}
static void fsF2_CLR1() {
    DIRECT_PAGE_BIT_OP( CLR, 7 );
}

static void fs60_CLRC() {
    CLR( &PSW, SPC_CARRY_FLAG );
}
static void fs20_CLRP() {
    CLR( &PSW, SPC_P_FLAG );
}
static void fsE0_CLRV() {
    CLR( &PSW, SPC_BREAK_FLAG );
}
#pragma endregion 

#pragma region SPC_CMP
static void CMP( uint8_t *O1, uint8_t *O2 ) {
    // TODO - verify
    int8_t sO1 = (int8_t)*O1;
    int8_t sO2 = (int8_t)*O2;
    int8_t result = sO1 - sO2;
    PSW = ( PSW & ~( SPC_ZERO_FLAG | SPC_CARRY_FLAG | SPC_NEGATIVE_FLAG ) )
        | ( ( result == 0 ) ? SPC_ZERO_FLAG
        : ( ( ( sO1 >= sO2 ) ? SPC_CARRY_FLAG : 0x00 )
        | ( ( result & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 ) ) );
}
static void CMPW( uint16_t *O1, uint8_t *O2 ) {
    // TODO - verify
    int16_t sO1 = (int16_t)*O1;
    int16_t sO2 = (int16_t)*O2;
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
static void DEC( uint8_t *O1 ) {
    --*O1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
}
static void INC( uint8_t *O1 ) {
    ++*O1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
}
static void fs9C_DEC() {
    IMPLIED_A_OP( DEC );
}
static void fs1D_DEC() {
    IMPLIED_X_OP( DEC );
}
static void fsDC_DEC() {
    IMPLIED_Y_OP( DEC );
}
static void fs8B_DEC() {
    DIRECT_D_OP( DEC );
}
static void fs9B_DEC() {
    X_INDEXED_DIRECT_PAGE_DX_OP( DEC );
}
static void fs8C_DEC() {
    ABSOLUTE_a_OP( DEC );
}
static void fs1A_DECW() {
    uint8_t *O1 = direct( 0 );
    uint16_t val = getWord( O1 ) - 1;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( val & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( val == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    
    setWord( O1, val );
}
static void fsBC_INC() {
    IMPLIED_A_OP( INC );
}
static void fs3D_INC() {
    IMPLIED_X_OP( INC );
}
static void fsFC_INC() {
    IMPLIED_Y_OP( INC );
}
static void fsAB_INC() {
    DIRECT_D_OP( INC );
}
static void fsBB_INC() {
    X_INDEXED_DIRECT_PAGE_DX_OP( INC );
}
static void fsAC_INC() {
    ABSOLUTE_a_OP( INC );
}
static void fs3A_INCW() {
    uint8_t *O1 = direct( 0 );
    uint16_t val = getWord( O1 );
    ++val;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) )
        | ( ( val & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( val == 0 ) ? SPC_ZERO_FLAG : 0x00 );
    
    setWord( O1, val );
}
#pragma endregion 

#pragma region SPC_EOR_OR
static void EOR( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O1 ^ *O2;
}
static bool EOR1( bool O1, bool O2 ) {
    return O1 ^ O2;
}

static void OR( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O1 | *O2;
}
static bool OR1( bool O1, bool O2 ) {
    return O1 || O2;
}
static void fs59_EOR() {
    INDIRECT_PAGE_INDIRECT_PAGE_OP( EOR );
}
static void fs48_EOR() {
    IMMEDIATE_A_I_OP( EOR );
}
static void fs46_EOR() {
    INDIRECT_A_X_OP( EOR );
}
static void fs57_EOR() {
   INDIRECT_Y_INDEXED_A_dY_OP( EOR );
}
static void fs47_EOR() {
    X_INDEXED_INDIRECT_A_dX_OP( EOR );
}
static void fs44_EOR() {
    DIRECT_A_D_OP( EOR );
}
static void fs54_EOR() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( EOR );
}
static void fs45_EOR() {
    ABSOLUTE_A_a_OP( EOR );
}
static void fs55_EOR() {
    X_INDEXED_ABSOLUTE_A_aX_OP( EOR );
}
static void fs56_EOR() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( EOR );
}
static void fs49_EOR() {
    DIRECT_PAGE_DIRECT_PAGE_OP( EOR );
}
static void fs58_EOR() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( EOR );
}
static void fs8A_EOR1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( EOR1 );
}

static void fs19_OR() {
    INDIRECT_PAGE_INDIRECT_PAGE_OP( OR );
}
static void fs08_OR() {
    IMMEDIATE_A_I_OP( OR );
}
static void fs06_OR() {
    INDIRECT_A_X_OP( OR );
}
static void fs17_OR() {
   INDIRECT_Y_INDEXED_A_dY_OP( OR );
}
static void fs07_OR() {
    X_INDEXED_INDIRECT_A_dX_OP( OR );
}
static void fs04_OR() {
    DIRECT_A_D_OP( OR );
}
static void fs14_OR() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( OR );
}
static void fs05_OR() {
    ABSOLUTE_A_a_OP( OR );
}
static void fs15_OR() {
    X_INDEXED_ABSOLUTE_A_aX_OP( OR );
}
static void fs16_OR() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( OR );
}
static void fs09_OR() {
    DIRECT_PAGE_DIRECT_PAGE_OP( OR );
}
static void fs18_OR() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( OR );
}
static void fs2A_OR1() {
    ABSOLUTE_BOOLEAN_BIT_C_iMB_OP( OR1 );
}
static void fs0A_OR1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( OR1 );
}
#pragma endregion 

#pragma region SPC_MOV
// TODO - flags on/off
static void MOV( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O2;
}

static void MOV_FLAG( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O2;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
}

static bool MOV1( bool O1, bool O2 ) {
    O1 = O2; // TODO - disable warning on unused O1
    return O1;
}

static void MOVW_YA_D( uint16_t *O1, uint8_t *O2 ) {
    // TODO - make sure this is OK
    *O1 = getWord( O2 );
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( ( *O1 & 0x8000 ) ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 );
}

static void fsAF_MOV() {
    INDIRECT_AUTO_INC_X_A_OP( MOV );
}
static void fsC6_MOV() {
    INDIRECT_X_A_OP( MOV );
}
static void fsD7_MOV() {
    INDIRECT_Y_INDEXED_dY_A_OP( MOV );
}
static void fsC7_MOV() {
    X_INDEXED_INDIRECT_dX_A_OP( MOV );
}
static void fsE8_MOV() {
    IMMEDIATE_A_I_OP( MOV_FLAG );
}
static void fsE6_MOV() {
    INDIRECT_A_X_OP( MOV_FLAG );
}
static void fsBF_MOV() {
    INDIRECT_AUTO_INC_A_X_OP( MOV_FLAG );
}
static void fsF7_MOV() {
   INDIRECT_Y_INDEXED_A_dY_OP( MOV_FLAG );
}
static void fsE7_MOV() {
    X_INDEXED_INDIRECT_A_dX_OP( MOV_FLAG );
}
static void fs7D_MOV() {
    IMPLIED_A_X_OP( MOV_FLAG );
}
static void fsDD_MOV() {
    IMPLIED_A_Y_OP( MOV_FLAG );
}
static void fsE4_MOV() {
    DIRECT_A_D_OP( MOV_FLAG );
}
static void fsF4_MOV() {
    X_INDEXED_DIRECT_PAGE_A_DX_OP( MOV_FLAG );
}
static void fsE5_MOV() {
    ABSOLUTE_A_a_OP( MOV_FLAG );
}
static void fsF5_MOV() {
    X_INDEXED_ABSOLUTE_A_aX_OP( MOV_FLAG );
}
static void fsF6_MOV() {
    Y_INDEXED_ABSOLUTE_A_aY_OP( MOV_FLAG );
}
static void fsBD_MOV() {
    IMPLIED_SP_X_OP( MOV );
}
static void fsCD_MOV() {
    IMMEDIATE_X_I_OP( MOV_FLAG );
}
static void fs5D_MOV() {
    IMPLIED_X_A_OP( MOV_FLAG );
}
static void fs9D_MOV() {
    IMPLIED_X_SP_OP( MOV_FLAG );
}
static void fsF8_MOV() {
    DIRECT_X_D_OP( MOV_FLAG );
}
static void fsF9_MOV() {
    Y_INDEXED_DIRECT_PAGE_X_DY_OP( MOV_FLAG );
}
static void fsE9_MOV() {
    ABSOLUTE_X_a_OP( MOV_FLAG );
}
static void fs8D_MOV() {
    IMMEDIATE_Y_I_OP( MOV_FLAG );
}
static void fsFD_MOV() {
    IMPLIED_Y_A_OP( MOV_FLAG );
}
static void fsEB_MOV() {
    DIRECT_Y_D_OP( MOV_FLAG );
}
static void fsFB_MOV() {
    X_INDEXED_DIRECT_PAGE_Y_DX_OP( MOV_FLAG );
}
static void fsEC_MOV() {
    ABSOLUTE_Y_a_OP( MOV_FLAG );
}
static void fsFA_MOV() {
    DIRECT_PAGE_DIRECT_PAGE_OP( MOV );
}
static void fsD4_MOV() {
    X_INDEXED_DIRECT_PAGE_DX_A_OP( MOV );
}
static void fsDB_MOV() {
    X_INDEXED_DIRECT_PAGE_DX_Y_OP( MOV );
}
static void fsD9_MOV() {
    Y_INDEXED_DIRECT_PAGE_DY_X_OP( MOV );
}
static void fs8F_MOV() {
    IMMEDIATE_TO_DIRECT_PAGE_OP( MOV );
}
static void fsC4_MOV() {
    DIRECT_D_A_OP( MOV );
}
static void fsD8_MOV() {
    DIRECT_D_X_OP( MOV );
}
static void fsCB_MOV() {
    DIRECT_D_Y_OP( MOV );
}
static void fsD5_MOV() {
    X_INDEXED_ABSOLUTE_aX_A_OP( MOV );
}
static void fsD6_MOV() {
    Y_INDEXED_ABSOLUTE_aY_A_OP( MOV );
}
static void fsC5_MOV() {
    ABSOLUTE_a_A_OP( MOV );
}
static void fsC9_MOV() {
    ABSOLUTE_a_X_OP( MOV );
}
static void fsCC_MOV() {
    ABSOLUTE_a_Y_OP( MOV );
}
static void fsAA_MOV1() {
    ABSOLUTE_BOOLEAN_BIT_C_MB_OP( MOV1 );
}
static void fsCA_MOV1() {
    ABSOLUTE_BOOLEAN_BIT_MB_C_OP( MOV1 );
}
static void fsBA_MOVW() {
    DIRECT_YA_D_OP( MOVW_YA_D );
}
static void fsDA_MOVW() {
    setWord( direct( 0 ), getYA() );
}

#pragma endregion

#pragma region SPC_SET
static void SET( uint8_t *O1, uint8_t mask ) {
    *O1 |= mask;
}
static void fs02_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 0 );
}
static void fs22_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 1 );
}
static void fs42_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 2 );
}
static void fs62_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 3 );
}
static void fs82_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 4 );
}
static void fsA2_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 5 );
}
static void fsC2_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 6 );
}
static void fsE2_SET1() {
    DIRECT_PAGE_BIT_OP( SET, 7 );
}
static void fs80_SETC() {
    SET( &PSW, SPC_CARRY_FLAG );
}
static void fs40_SETP() {
    SET( &PSW, SPC_P_FLAG );
}
#pragma endregion

#pragma region SPC_TCALL
static void TCALL( uint16_t addr ) {
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
static void fs3F_CALL( uint8_t *O1 ) {
    uint8_t r = ( next_program_counter >> 8 ) & 0x00FF;
    PUSH( &r );
    r = next_program_counter & 0x00FF;
    PUSH( &r );
    next_program_counter = getWord( O1 );
}

static void CBNE( uint8_t* O1, uint8_t *R ) {
    if ( A == *O1 ){
        // TODO - should we be getting a word here?
        next_program_counter = getWord( R );
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

static void DBNZ( uint8_t *O1, uint8_t *O2 ) {
    --*O1;
    if ( *O1 != 0 ) {
        next_program_counter = curr_program_counter + *O2;
        opCycles += 1;
    }
}
static void fsFE_DBNZ() {
    RELATIVE_OP_Y_R( DBNZ );
}
static void fs6E_DBNZ() {
    RELATIVE_OP_D_R( DBNZ )
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
    CLR( &PSW, SPC_INTERRUPT_FLAG );
}
static void fsA0_EI() {
    SET( &PSW, SPC_INTERRUPT_FLAG );
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

static bool NOT1( bool O1 ) {
    return !O1;
}

static void fsEA_NOT1() {
    ABSOLUTE_BOOLEAN_BIT_MB_OP( NOT1 );
}
static void fsED_NOTC() {
    PSW = PSW ^ SPC_CARRY_FLAG;
}

static void fs4F_PCALL() {
    next_program_counter = 0xFF00 + immediate_8();
}
static void fs6F_RET() {
    uint8_t h, l;
    POP( &l );
    POP( &h );
    next_program_counter = ( ( (uint16_t)h ) << 8 ) | (uint16_t) l;
}
static void fs7F_RET1() {
    POP( &PSW );
    fs6F_RET();
}
static void fsEF_SLEEP() {
    // TODO
}

static void fsFF_STOP() {
    // TODO
}

static uint8_t *TSETCLR_BASE() {
    uint8_t *a = absolute( 0 );
    uint8_t temp = *a & A;
    PSW = ( PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) ) 
        | ( temp & 0x80 ? SPC_NEGATIVE_FLAG : 0x00 )
        | ( temp == 0 ? SPC_ZERO_FLAG : 0x00 );
    
    return a;
}

static void fs4E_TCLR1() {
    uint8_t *a = TSETCLR_BASE();
    *a &= ~A;
}
static void fs0E_TSET1() {
    uint8_t *a = TSETCLR_BASE();
    *a |= A;
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