
#include "spc700.h"
#include "spc700_instruction_declarations.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

//void(*spc700_instructions[16 * 16])();

//SPC700InstructionEntry instructions[ 0xFF ];

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

/* Memory and registers */
static uint8_t spc_memory[ 0xFFFF + 0x01 ];
static uint16_t program_counter, next_program_counter, curr_program_counter;
static uint8_t A, X, Y, SP, PSW;
static Registers* registers = ( spc_memory + 0X00F0 );

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

/* 16 bit stack pointer = 0x0100 | SP_register */
static uint16_t get_stack_pointer() {
    uint16_t toRet = SP;
    return toRet | 0x01;
}

/* Initialise (power on) */
void spc700_initialise() {
    registers->port0 = 0xAA;
    registers->port1 = 0xBB;
    SP = 0xEF;
}

/* Access the SPC memory */
uint8_t *get_spc_memory() {
    return spc_memory;
}

uint8_t *spc_memory_map( uint16_t addr ) {
    if ( addr > 0x0000 && addr < 0x00EF ) {
        return &spc_memory[ addr ];
    }
    else if ( addr < 0x00FF ) {
        // Registers
        return &spc_memory[ addr ];
    }
    else if ( addr < 0x01FF ) {
        // Page 1 and Memory
        return &spc_memory[ addr ];
    }
    else if ( addr < 0xFFFF ) {
        // Memory (Read/ReadWrite)/IPL Rom
        // TODO - adjust based on X reg
        return &spc_memory[ addr - 0x0F ];
    }
}

uint8_t *accessPageAddr( uint8_t addr ) {
    return spc_memory_map( ( ( PSW & SPC_P_FLAG ) ? 0x0100 : 0x0000 ) + addr );
}

/* Access the 4 visible bytes from the CPU */
uint8_t *access_spc_snes_mapped( uint16_t addr ) {
    addr = addr - 0x2140 + 0x00f4;
    
    if ( addr < 0x00f4 || addr > 0x00f7 ) {
        return NULL;
    }
    
    return spc_memory_map( addr );
}

/* Execute next instruction, update PC and cycle counter etc */
void spc700_execute_next_instruction() {
    curr_program_counter = program_counter;
    SPC700InstructionEntry *entry = &instructions[ *spc_memory_map( program_counter++ ) ];
    next_program_counter = curr_program_counter + entry->opLength;
    entry->instruction();

    // Operation may mutate next_program_counter if it branches/jumps
    program_counter = next_program_counter;
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
static uint8_t indirect_X_8() {
    return *indirect_X();
}
static uint8_t indirect_X_AI_8() {
    uint8_t toRet = *indirect_X();
    ++X;
    return toRet;
}
static uint8_t *indirect_Y() {
    return accessPageAddr( Y );
}
static uint8_t indirect_Y_8() {
    return *indirect_Y();
}
static uint8_t indirect_Y_AI_8() {
    uint8_t toRet = *indirect_Y();
    ++Y;
    return toRet;
}

static uint8_t *direct( uint8_t offset ) {
    return accessPageAddr( immediate_8() + offset );
}
static uint8_t direct_8( uint8_t offset ) {
    return *direct( offset );
}
static uint16_t direct_16( uint8_t offset ) {
    return getWord( direct( offset ) );
}
static uint8_t direct_indexed_X_8() {
    return *direct( X );
}
static uint16_t direct_indexed_X_16() {
    return getWord( direct( X ) );
}
static uint8_t direct_indexed_Y_8() {
    return *direct( Y );
}
static uint16_t direct_indexed_Y_16() {
    return getWord( direct( Y ) );
}

static uint8_t *direct_indexed_X_indirect() {
    return spc_memory_map( direct_indexed_X_16() );
}
static uint8_t direct_indexed_X_indirect_8() {
    return *direct_indexed_X_indirect();
}

static uint8_t *x_indexed_indirect() {
    return spc_memory_map( direct_16( X ) );
}
static uint8_t *x_indexed_indirect_8() {
    return *spc_memory_map( direct_16( X ) );
}

static uint8_t *indirect_y_indexed() {
    return spc_memory_map( direct_16( 0 ) + Y );
}

static uint8_t indirect_y_indexed_8() {
    return *indirect_y_indexed;
}

static uint16_t absolute_addr() {
    uint16_t word = getWord( spc_memory_map( program_counter ) );
    program_counter += 2;
    return word;
}
static uint8_t *absolute( uint8_t offset ) {
    return spc_memory_map( absolute_addr() + offset );
}
static uint8_t absolute_8() {
    return *absolute( 0 );
}
static uint16_t absolute_16() {
    return getWord( absolute( 0 ) );
}
static uint8_t absolute_membit( uint8_t **mem ) {
    uint16_t operand = absolute_addr();
    uint8_t bitLoc = ( operand >> 13 ) & 0x0007;
    uint16_t addr = operand & 0x1FFF;
    *mem = spc_memory_map( addr );
    return bitLoc;
}

static uint8_t *absolute_index( uint8_t reg ) {
    return spc_memory_map( absolute_addr() + reg );
}

static uint8_t absolute_indexed_X_8(){
    return *absolute_index( X );
}
static uint8_t absolute_indexed_Y_8() {
    return *absolute_index( Y );
}

static uint8_t stack_pop_8(){
    SP -= 1;
    return spc_memory[ SP ];
}

// TODO - increment PC as data consumed (fix param ordering)

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

#define X_INDEXED_DIRECT_PAGE_DX_R_OP( op ) \
    uint8_t nearLabel = immediate_8(); \
    op( direct( X ), &nearLabel );
    // TODO - ordering \
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
    op( indirect_X(), A );
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
// TODO - make sure we're getting DD/DS right
#define DIRECT_PAGE_DIRECT_PAGE_OP( op ) \
    uint8_t *dd = direct( 0 ); \
    uint8_t *ds = direct( 0 ); \
    op( dd, ds ); \
// DIRECT PAGE TO DIRECT PAGE OPS END


// INDIRECT PAGE TO INDIRECT PAGE OPS START
// TODO - param ordering
#define INDIRECT_PAGE_INDIRECT_PAGE_OP( op ) \
    op( indirect_X(), indirect_Y() ); \
// INDIRECT PAGE TO INDIRECT PAGE OPS END

// IMMEDIATE TO DIRECT PAGE OPS START
// TODO - param ordering
#define IMMEDIATE_TO_DIRECT_PAGE_OP( op ) \
    op( direct( 0 ), immediate() );
// IMMEDIATE TO DIRECT PAGE OPS END

// DIRECT PAGE BIT OPS START
#define DIRECT_PAGE_BIT_OP( op, bit ) \
    op( direct( 0 ), 0x01 << bit );
// DIRECT PAGE BIT OPS END

// DIRECT PAGE BIT RELATIVE OPS START
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

#define RELATIVE_OP_D_R( op ) \
    uint8_t *d = direct( 0 ); \
    uint8_t *r = immediate(); \
    op( d, r ); // TODO - ordering

#define RELATIVE_OP_Y_R( op ) \
    op( &Y, immediate() );
// RELATIVE OPS END

// IMMEDIATE OPS START
#define IMMEDIATE_A_I_OP( op ) \
    op( &A, immediate() );

#define IMMEDIATE_X_I_OP( op ) \
    op( &X, immediate() );

#define IMMEDIATE_Y_I_OP( op ) \
    op( &Y, immediate_8() );
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
    *O1 = accessPageAddr( ++SP );
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

static void SBC( uint8_t *A, uint8_t *B ) {
    uint8_t Bneg = ( ~( *B + ( PSW & SPC_CARRY_FLAG ? 0 : 1 ) ) ) + 1;
    ADC( A, &Bneg );
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
    if ( YA & 0xFF < O1 & 0xFF ) {
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

    PSW = PSW
        & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG )
        | ( ( *O1 == 0 ) ? SPC_ZERO_FLAG : 0x00 ) 
        | ( ( *O1 & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static bool AND1( bool A, bool B ) {
    return A && B;
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
static void BBC( bool O1, uint8_t* r ) {
    if ( O1 ) {
        program_counter += *r;
    }
}

static void BBS( bool O1, uint8_t* r ) {
    if ( O1 ) {
        program_counter += *r;
    }
}

static void JMP( uint16_t addr ) {
    program_counter = addr;
}

static void BranchOnCondition( bool condition )
{
    uint8_t r = immediate_8();
    if ( condition )
        program_counter += r;
    
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
    BranchOnCondition( !( PSW & SPC_CARRY_FLAG ) );
}
static void fsB0_BCS() {
    BranchOnCondition( PSW & SPC_CARRY_FLAG );
}
static void fsF0_BEQ() {
    BranchOnCondition( PSW & SPC_ZERO_FLAG );
}
static void fs30_BMI() {
    BranchOnCondition( PSW & SPC_NEGATIVE_FLAG );
}
static void fsD0_BNE() {
    BranchOnCondition( !( PSW & SPC_ZERO_FLAG ) );
}
static void fs10_BPL() {
    BranchOnCondition( !( PSW & SPC_NEGATIVE_FLAG ) );
}
static void fs50_BVC() {
    BranchOnCondition( !( PSW & SPC_OVERFLOW_FLAG ) );
}
static void fs70_BVS() {
    BranchOnCondition( PSW & SPC_OVERFLOW_FLAG );
}
static void fs2F_BRA() {
    BranchOnCondition( true );
}
static void fs0F_BRK() {
    // TODO
    PSW |= SPC_BREAK_FLAG;
    uint8_t r = ( program_counter >> 8 ) & 0x00FF;
    PUSH( &r );
    r = program_counter & 0x00FF;
    PUSH( &r );
    PUSH( PSW );

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
    // TODO
}
static void CMPW( uint16_t *O1, uint8_t *O2 ) {
    // TODO
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
static void DEC(uint8_t B) {

}
static void INC(uint8_t B) {

}
static void fs9C_DEC() {
}
static void fs1D_DEC() {
}
static void fsDC_DEC() {
}
static void fs8B_DEC() {
}
static void fs9B_DEC() {
}
static void fs8C_DEC() {
}
static void fs1A_DECW() {
}
static void fsBC_INC() {
}
static void fs3D_INC() {
}
static void fsFC_INC() {
}
static void fsAB_INC() {
}
static void fsBB_INC() {
}
static void fsAC_INC() {
}
static void fs3A_INCW() {
}
#pragma endregion 

#pragma region SPC_EOR_OR
static void EOR( uint8_t *O1, uint8_t *O2 ) {
    // TODO 
    return *O1 ^ *O2;
}
static bool EOR1( bool O1, bool O2 ) {
    // TODO
    return O1 ^ O2;
}

static void OR( uint8_t *O1, uint8_t *O2 ) {
    // TODO
    return *O1 | *O2;
}
static bool OR1( bool O1, bool O2 ) {
    // TODO
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
    return O2;
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
    uint8_t r = ( program_counter >> 8 ) & 0x00FF;
    PUSH( &r );
    r = program_counter & 0x00FF;
    PUSH( &r );
    program_counter = getWord( O1 );
}

static void CBNE( uint8_t* O1, uint8_t *R ) {
    if ( A == *O1 )
        program_counter = getWord( R );
}
static void fsDE_CBNE() {
    X_INDEXED_DIRECT_PAGE_DX_R_OP( CBNE );
}
static void fs2E_CBNE() {
    RELATIVE_OP_D_R( CBNE );
}
static void fsDF_DAA() {
    // TODO - verify these
    if ( PSW & SPC_CARRY_FLAG || A > 0x99 ) {
       PSW |= SPC_CARRY_FLAG;
       A += 0x60;
    }
    
    if ( PSW & SPC_HALF_CARRY_FLAG || ( A & 0x0F ) > 0x09 ) {
       A += 0x06;
    }

    PSW = PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) 
        | ( ( A == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( A & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );

}
static void fsBE_DAS() {
    if ( !( PSW & SPC_CARRY_FLAG ) || A > 0x99 ) {
       PSW &= ~SPC_CARRY_FLAG;
       A -= 0x60;
    }
    
    if (!( PSW & SPC_HALF_CARRY_FLAG ) || ( A & 0x0F ) > 0x09 ) {
       A -= 0x06;
    }
  
    PSW = PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) 
        | ( ( A == 0 ) ? SPC_ZERO_FLAG : 0x00 )
        | ( ( A & 0x80 ) ? SPC_NEGATIVE_FLAG : 0x00 );
}

static void DBNZ( uint8_t *O1, uint8_t *O2 ) {
    --*O1;
    if ( *O1 != 0 ) {
        program_counter += *O2;
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
    PSW = PSW & ~( SPC_NEGATIVE_FLAG | SPC_ZERO_FLAG ) 
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
    program_counter = 0xFF00 + immediate_8();
}
static void fs6F_RET() {
    uint8_t h, l;
    POP( &l );
    POP( &h );
    program_counter = ( ( (uint16_t)h ) << 8 ) | (uint16_t) l;
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
    uint8_t a = 10 / 0; // Technically works
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

void spc700_populate_instructions() {

    instructions[0x00] = (SPC700InstructionEntry) { fs00_NOP, 1, 2 };	// ........ :  : do nothing
    instructions[0x01] = (SPC700InstructionEntry) { fs01_TCALL, 1, 8 };	// ........ : 0        : CALL [$FFDE]
    instructions[0x02] = (SPC700InstructionEntry) { fs02_SET1, 2, 4 };	// ........ : d.0      : d.0 = 1
    instructions[0x03] = (SPC700InstructionEntry) { fs03_BBS, 3, 5 };	// ........ : d.0, r   : PC+=r  if d.0 == 1
    instructions[0x04] = (SPC700InstructionEntry) { fs04_OR, 2, 3 };	// N.....Z. : A, d     : A = A | (d)
    instructions[0x05] = (SPC700InstructionEntry) { fs05_OR, 3, 4 };	// N.....Z. : A, !a    : A = A | (a)
    instructions[0x06] = (SPC700InstructionEntry) { fs06_OR, 1, 3 };	// N.....Z. : A, (X)   : A = A | (X)
    instructions[0x07] = (SPC700InstructionEntry) { fs07_OR, 2, 6 };	// N.....Z. : A, [d+X] : A = A | ([d+X])
    instructions[0x08] = (SPC700InstructionEntry) { fs08_OR, 2, 2 };	// N.....Z. : A, #i    : A = A | i
    instructions[0x09] = (SPC700InstructionEntry) { fs09_OR, 3, 6 };	// N.....Z. : dd, ds   : (dd) = (dd) | (ds)
    instructions[0x0A] = (SPC700InstructionEntry) { fs0A_OR1, 3, 5 };	// .......C : C, m.b   : C = C | (m.b)
    instructions[0x0B] = (SPC700InstructionEntry) { fs0B_ASL, 2, 4 };	// N.....ZC : d        : Left shift (d) as above
    instructions[0x0C] = (SPC700InstructionEntry) { fs0C_ASL, 3, 5 };	// N.....ZC : !a       : Left shift (a) as above
    instructions[0x0D] = (SPC700InstructionEntry) { fs0D_PUSH, 1, 4 };	// ........ : PSW      : (SP--) = Flags
    instructions[0x0E] = (SPC700InstructionEntry) { fs0E_TSET1, 3, 6 };	// N.....Z. : !a       : (a) = (a)|A, ZN as for A-(a)
    instructions[0x0F] = (SPC700InstructionEntry) { fs0F_BRK, 1, 8 };	// ...1.0.. :  : Push PC and Flags, PC = [$FFDE]

    instructions[0x10] = (SPC700InstructionEntry) { fs10_BPL, 2, 2 };	// ........ : r        : PC+=r  if N == 0
    instructions[0x11] = (SPC700InstructionEntry) { fs11_TCALL, 1, 8 };	// ........ : 1        : CALL [$FFDC]
    instructions[0x12] = (SPC700InstructionEntry) { fs12_CLR1, 2, 4 };	// ........ : d.0      : d.0 = 0
    instructions[0x13] = (SPC700InstructionEntry) { fs13_BBC, 3, 5 };	// ........ : d.0, r   : PC+=r  if d.0 == 0
    instructions[0x14] = (SPC700InstructionEntry) { fs14_OR, 2, 4 };	// N.....Z. : A, d+X   : A = A | (d+X)
    instructions[0x15] = (SPC700InstructionEntry) { fs15_OR, 3, 5 };	// N.....Z. : A, !a+X  : A = A | (a+X)
    instructions[0x16] = (SPC700InstructionEntry) { fs16_OR, 3, 5 };	// N.....Z. : A, !a+Y  : A = A | (a+Y)
    instructions[0x17] = (SPC700InstructionEntry) { fs17_OR, 2, 6 };	// N.....Z. : A, [d]+Y : A = A | ([d]+Y)
    instructions[0x18] = (SPC700InstructionEntry) { fs18_OR, 3, 5 };	// N.....Z. : d, #i    : (d) = (d) | i
    instructions[0x19] = (SPC700InstructionEntry) { fs19_OR, 1, 5 };	// N.....Z. : (X), (Y) : (X) = (X) | (Y)
    instructions[0x1A] = (SPC700InstructionEntry) { fs1A_DECW, 2, 6 };	// N.....Z. : d        : Word (d)--
    instructions[0x1B] = (SPC700InstructionEntry) { fs1B_ASL, 2, 5 };	// N.....ZC : d+X      : Left shift (d+X) as above
    instructions[0x1C] = (SPC700InstructionEntry) { fs1C_ASL, 1, 2 };	// N.....ZC : A        : Left shift A: high->C, 0->low
    instructions[0x1D] = (SPC700InstructionEntry) { fs1D_DEC, 1, 2 };	// N.....Z. : X        : X--
    instructions[0x1E] = (SPC700InstructionEntry) { fs1E_CMP, 3, 4 };	// N.....ZC : X, !a    : X - (a)
    instructions[0x1F] = (SPC700InstructionEntry) { fs1F_JMP, 3, 6 };	// ........ : [!a+X]   : PC = [a+X]

    instructions[0x20] = (SPC700InstructionEntry) { fs20_CLRP, 1, 2 };	// ..0..... :  : P = 0
    instructions[0x21] = (SPC700InstructionEntry) { fs21_TCALL, 1, 8 };	// ........ : 2        : CALL [$FFDA]
    instructions[0x22] = (SPC700InstructionEntry) { fs22_SET1, 2, 4 };	// ........ : d.1      : d.1 = 1
    instructions[0x23] = (SPC700InstructionEntry) { fs23_BBS, 3, 5 };	// ........ : d.1, r   : PC+=r  if d.1 == 1
    instructions[0x24] = (SPC700InstructionEntry) { fs24_AND, 2, 3 };	// N.....Z. : A, d     : A = A & (d)
    instructions[0x25] = (SPC700InstructionEntry) { fs25_AND, 3, 4 };	// N.....Z. : A, !a    : A = A & (a)
    instructions[0x26] = (SPC700InstructionEntry) { fs26_AND, 1, 3 };	// N.....Z. : A, (X)   : A = A & (X)
    instructions[0x27] = (SPC700InstructionEntry) { fs27_AND, 2, 6 };	// N.....Z. : A, [d+X] : A = A & ([d+X])
    instructions[0x28] = (SPC700InstructionEntry) { fs28_AND, 2, 2 };	// N.....Z. : A, #i    : A = A & i
    instructions[0x29] = (SPC700InstructionEntry) { fs29_AND, 3, 6 };	// N.....Z. : dd, ds   : (dd) = (dd) & (ds)
    instructions[0x2A] = (SPC700InstructionEntry) { fs2A_OR1, 3, 5 };	// .......C : C, /m.b  : C = C | ~(m.b)
    instructions[0x2B] = (SPC700InstructionEntry) { fs2B_ROL, 2, 4 };	// N.....ZC : d        : Left shift (d) as above
    instructions[0x2C] = (SPC700InstructionEntry) { fs2C_ROL, 3, 5 };	// N.....ZC : !a       : Left shift (a) as above
    instructions[0x2D] = (SPC700InstructionEntry) { fs2D_PUSH, 1, 4 };	// ........ : A        : (SP--) = A
    instructions[0x2E] = (SPC700InstructionEntry) { fs2E_CBNE, 3, 5 };	// ........ : d, r     : CMP A, (d) then BNE
    instructions[0x2F] = (SPC700InstructionEntry) { fs2F_BRA, 2, 4 };	// ........ : r        : PC+=r

    instructions[0x30] = (SPC700InstructionEntry) { fs30_BMI, 2, 2 };	// ........ : r        : PC+=r  if N == 1
    instructions[0x31] = (SPC700InstructionEntry) { fs31_TCALL, 1, 8 };	// ........ : 3        : CALL [$FFD8]
    instructions[0x32] = (SPC700InstructionEntry) { fs32_CLR1, 2, 4 };	// ........ : d.1      : d.1 = 0
    instructions[0x33] = (SPC700InstructionEntry) { fs33_BBC, 3, 5 };	// ........ : d.1, r   : PC+=r  if d.1 == 0
    instructions[0x34] = (SPC700InstructionEntry) { fs34_AND, 2, 4 };	// N.....Z. : A, d+X   : A = A & (d+X)
    instructions[0x35] = (SPC700InstructionEntry) { fs35_AND, 3, 5 };	// N.....Z. : A, !a+X  : A = A & (a+X)
    instructions[0x36] = (SPC700InstructionEntry) { fs36_AND, 3, 5 };	// N.....Z. : A, !a+Y  : A = A & (a+Y)
    instructions[0x37] = (SPC700InstructionEntry) { fs37_AND, 2, 6 };	// N.....Z. : A, [d]+Y : A = A & ([d]+Y)
    instructions[0x38] = (SPC700InstructionEntry) { fs38_AND, 3, 5 };	// N.....Z. : d, #i    : (d) = (d) & i
    instructions[0x39] = (SPC700InstructionEntry) { fs39_AND, 1, 5 };	// N.....Z. : (X), (Y) : (X) = (X) & (Y)
    instructions[0x3A] = (SPC700InstructionEntry) { fs3A_INCW, 2, 6 };	// N.....Z. : d        : Word (d)++
    instructions[0x3B] = (SPC700InstructionEntry) { fs3B_ROL, 2, 5 };	// N.....ZC : d+X      : Left shift (d+X) as above
    instructions[0x3C] = (SPC700InstructionEntry) { fs3C_ROL, 1, 2 };	// N.....ZC : A        : Left shift A: low=C, C=high
    instructions[0x3D] = (SPC700InstructionEntry) { fs3D_INC, 1, 2 };	// N.....Z. : X        : X++
    instructions[0x3E] = (SPC700InstructionEntry) { fs3E_CMP, 2, 3 };	// N.....ZC : X, d     : X - (d)
    instructions[0x3F] = (SPC700InstructionEntry) { fs3F_CALL, 3, 8 };	// ........ : !a       : (SP--)=PCh, (SP--)=PCl, PC=a

    instructions[0x40] = (SPC700InstructionEntry) { fs40_SETP, 1, 2 };	// ..1..... :  : P = 1
    instructions[0x41] = (SPC700InstructionEntry) { fs41_TCALL, 1, 8 };	// ........ : 4        : CALL [$FFD6]
    instructions[0x42] = (SPC700InstructionEntry) { fs42_SET1, 2, 4 };	// ........ : d.2      : d.2 = 1
    instructions[0x43] = (SPC700InstructionEntry) { fs43_BBS, 3, 5 };	// ........ : d.2, r   : PC+=r  if d.2 == 1
    instructions[0x44] = (SPC700InstructionEntry) { fs44_EOR, 2, 3 };	// N.....Z. : A, d     : A = A EOR (d)
    instructions[0x45] = (SPC700InstructionEntry) { fs45_EOR, 3, 4 };	// N.....Z. : A, !a    : A = A EOR (a)
    instructions[0x46] = (SPC700InstructionEntry) { fs46_EOR, 1, 3 };	// N.....Z. : A, (X)   : A = A EOR (X)
    instructions[0x47] = (SPC700InstructionEntry) { fs47_EOR, 2, 6 };	// N.....Z. : A, [d+X] : A = A EOR ([d+X])
    instructions[0x48] = (SPC700InstructionEntry) { fs48_EOR, 2, 2 };	// N.....Z. : A, #i    : A = A EOR i
    instructions[0x49] = (SPC700InstructionEntry) { fs49_EOR, 3, 6 };	// N.....Z. : dd, ds   : (dd) = (dd) EOR (ds)
    instructions[0x4A] = (SPC700InstructionEntry) { fs4A_AND1, 3, 4 };	// .......C : C, m.b   : C = C & (m.b)
    instructions[0x4B] = (SPC700InstructionEntry) { fs4B_LSR, 2, 4 };	// N.....ZC : d        : Right shift (d) as above
    instructions[0x4C] = (SPC700InstructionEntry) { fs4C_LSR, 3, 5 };	// N.....ZC : !a       : Right shift (a) as above
    instructions[0x4D] = (SPC700InstructionEntry) { fs4D_PUSH, 1, 4 };	// ........ : X        : (SP--) = X
    instructions[0x4E] = (SPC700InstructionEntry) { fs4E_TCLR1, 3, 6 };	// N.....Z. : !a       : (a) = (a)&~A, ZN as for A-(a)
    instructions[0x4F] = (SPC700InstructionEntry) { fs4F_PCALL, 2, 6 };	// ........ : u        : CALL $FF00+u

    instructions[0x50] = (SPC700InstructionEntry) { fs50_BVC, 2, 2 };	// ........ : r        : PC+=r  if V == 0
    instructions[0x51] = (SPC700InstructionEntry) { fs51_TCALL, 1, 8 };	// ........ : 5        : CALL [$FFD4]
    instructions[0x52] = (SPC700InstructionEntry) { fs52_CLR1, 2, 4 };	// ........ : d.2      : d.2 = 0
    instructions[0x53] = (SPC700InstructionEntry) { fs53_BBC, 3, 5 };	// ........ : d.2, r   : PC+=r  if d.2 == 0
    instructions[0x54] = (SPC700InstructionEntry) { fs54_EOR, 2, 4 };	// N.....Z. : A, d+X   : A = A EOR (d+X)
    instructions[0x55] = (SPC700InstructionEntry) { fs55_EOR, 3, 5 };	// N.....Z. : A, !a+X  : A = A EOR (a+X)
    instructions[0x56] = (SPC700InstructionEntry) { fs56_EOR, 3, 5 };	// N.....Z. : A, !a+Y  : A = A EOR (a+Y)
    instructions[0x57] = (SPC700InstructionEntry) { fs57_EOR, 2, 6 };	// N.....Z. : A, [d]+Y : A = A EOR ([d]+Y)
    instructions[0x58] = (SPC700InstructionEntry) { fs58_EOR, 3, 5 };	// N.....Z. : d, #i    : (d) = (d) EOR i
    instructions[0x59] = (SPC700InstructionEntry) { fs59_EOR, 1, 5 };	// N.....Z. : (X), (Y) : (X) = (X) EOR (Y)
    instructions[0x5A] = (SPC700InstructionEntry) { fs5A_CMPW, 2, 4 };	// N.....ZC : YA, d    : YA - (d)
    instructions[0x5B] = (SPC700InstructionEntry) { fs5B_LSR, 2, 5 };	// N.....ZC : d+X      : Right shift (d+X) as above
    instructions[0x5C] = (SPC700InstructionEntry) { fs5C_LSR, 1, 2 };	// N.....ZC : A        : Right shift A: 0->high, low->C
    instructions[0x5D] = (SPC700InstructionEntry) { fs5D_MOV, 1, 2 };	// N.....Z. : X, A     : X = A
    instructions[0x5E] = (SPC700InstructionEntry) { fs5E_CMP, 3, 4 };	// N.....ZC : Y, !a    : Y - (a)
    instructions[0x5F] = (SPC700InstructionEntry) { fs5F_JMP, 3, 3 };	// ........ : !a       : PC = a

    instructions[0x60] = (SPC700InstructionEntry) { fs60_CLRC, 1, 2 };	// .......0 :  : C = 0
    instructions[0x61] = (SPC700InstructionEntry) { fs61_TCALL, 1, 8 };	// ........ : 6        : CALL [$FFD2]
    instructions[0x62] = (SPC700InstructionEntry) { fs62_SET1, 2, 4 };	// ........ : d.3      : d.3 = 1
    instructions[0x63] = (SPC700InstructionEntry) { fs63_BBS, 3, 5 };	// ........ : d.3, r   : PC+=r  if d.3 == 1
    instructions[0x64] = (SPC700InstructionEntry) { fs64_CMP, 2, 3 };	// N.....ZC : A, d     : A - (d)
    instructions[0x65] = (SPC700InstructionEntry) { fs65_CMP, 3, 4 };	// N.....ZC : A, !a    : A - (a)
    instructions[0x66] = (SPC700InstructionEntry) { fs66_CMP, 1, 3 };	// N.....ZC : A, (X)   : A - (X)
    instructions[0x67] = (SPC700InstructionEntry) { fs67_CMP, 2, 6 };	// N.....ZC : A, [d+X] : A - ([d+X])
    instructions[0x68] = (SPC700InstructionEntry) { fs68_CMP, 2, 2 };	// N.....ZC : A, #i    : A - i
    instructions[0x69] = (SPC700InstructionEntry) { fs69_CMP, 3, 6 };	// N.....ZC : dd, ds   : (dd) - (ds)
    instructions[0x6A] = (SPC700InstructionEntry) { fs6A_AND1, 3, 4 };	// .......C : C, /m.b  : C = C & ~(m.b)
    instructions[0x6B] = (SPC700InstructionEntry) { fs6B_ROR, 2, 4 };	// N.....ZC : d        : Right shift (d) as above
    instructions[0x6C] = (SPC700InstructionEntry) { fs6C_ROR, 3, 5 };	// N.....ZC : !a       : Right shift (a) as above
    instructions[0x6D] = (SPC700InstructionEntry) { fs6D_PUSH, 1, 4 };	// ........ : Y        : (SP--) = Y
    instructions[0x6E] = (SPC700InstructionEntry) { fs6E_DBNZ, 3, 5 };	// ........ : d, r     : (d)-- then JNZ
    instructions[0x6F] = (SPC700InstructionEntry) { fs6F_RET, 1, 5 };	// ........ :  : Pop PC

    instructions[0x70] = (SPC700InstructionEntry) { fs70_BVS, 2, 2 };	// ........ : r        : PC+=r  if V == 1
    instructions[0x71] = (SPC700InstructionEntry) { fs71_TCALL, 1, 8 };	// ........ : 7        : CALL [$FFD0]
    instructions[0x72] = (SPC700InstructionEntry) { fs72_CLR1, 2, 4 };	// ........ : d.3      : d.3 = 0
    instructions[0x73] = (SPC700InstructionEntry) { fs73_BBC, 3, 5 };	// ........ : d.3, r   : PC+=r  if d.3 == 0
    instructions[0x74] = (SPC700InstructionEntry) { fs74_CMP, 2, 4 };	// N.....ZC : A, d+X   : A - (d+X)
    instructions[0x75] = (SPC700InstructionEntry) { fs75_CMP, 3, 5 };	// N.....ZC : A, !a+X  : A - (a+X)
    instructions[0x76] = (SPC700InstructionEntry) { fs76_CMP, 3, 5 };	// N.....ZC : A, !a+Y  : A - (a+Y)
    instructions[0x77] = (SPC700InstructionEntry) { fs77_CMP, 2, 6 };	// N.....ZC : A, [d]+Y : A - ([d]+Y)
    instructions[0x78] = (SPC700InstructionEntry) { fs78_CMP, 3, 5 };	// N.....ZC : d, #i    : (d) - i
    instructions[0x79] = (SPC700InstructionEntry) { fs79_CMP, 1, 5 };	// N.....ZC : (X), (Y) : (X) - (Y)
    instructions[0x7A] = (SPC700InstructionEntry) { fs7A_ADDW, 2, 5 };	// NV..H.ZC : YA, d    : YA  = YA + (d), H on high byte
    instructions[0x7B] = (SPC700InstructionEntry) { fs7B_ROR, 2, 5 };	// N.....ZC : d+X      : Right shift (d+X) as above
    instructions[0x7C] = (SPC700InstructionEntry) { fs7C_ROR, 1, 2 };	// N.....ZC : A        : Right shift A: high=C, C=low
    instructions[0x7D] = (SPC700InstructionEntry) { fs7D_MOV, 1, 2 };	// N.....Z. : A, X     : A = X
    instructions[0x7E] = (SPC700InstructionEntry) { fs7E_CMP, 2, 3 };	// N.....ZC : Y, d     : Y - (d)
    instructions[0x7F] = (SPC700InstructionEntry) { fs7F_RET1, 1, 6 };	// NVPBHIZC :  : Pop Flags, PC

    instructions[0x80] = (SPC700InstructionEntry) { fs80_SETC, 1, 2 };	// .......1 :  : C = 1
    instructions[0x81] = (SPC700InstructionEntry) { fs81_TCALL, 1, 8 };	// ........ : 8        : CALL [$FFCE]
    instructions[0x82] = (SPC700InstructionEntry) { fs82_SET1, 2, 4 };	// ........ : d.4      : d.4 = 1
    instructions[0x83] = (SPC700InstructionEntry) { fs83_BBS, 3, 5 };	// ........ : d.4, r   : PC+=r  if d.4 == 1
    instructions[0x84] = (SPC700InstructionEntry) { fs84_ADC, 2, 3 };	// NV..H.ZC : A, d     : A = A+(d)+C
    instructions[0x85] = (SPC700InstructionEntry) { fs85_ADC, 3, 4 };	// NV..H.ZC : A, !a    : A = A+(a)+C
    instructions[0x86] = (SPC700InstructionEntry) { fs86_ADC, 1, 3 };	// NV..H.ZC : A, (X)   : A = A+(X)+C
    instructions[0x87] = (SPC700InstructionEntry) { fs87_ADC, 2, 6 };	// NV..H.ZC : A, [d+X] : A = A+([d+X])+C
    instructions[0x88] = (SPC700InstructionEntry) { fs88_ADC, 2, 2 };	// NV..H.ZC : A, #i    : A = A+i+C
    instructions[0x89] = (SPC700InstructionEntry) { fs89_ADC, 3, 6 };	// NV..H.ZC : dd, ds   : (dd) = (dd)+(d)+C
    instructions[0x8A] = (SPC700InstructionEntry) { fs8A_EOR1, 3, 5 };	// .......C : C, m.b   : C = C EOR (m.b)
    instructions[0x8B] = (SPC700InstructionEntry) { fs8B_DEC, 2, 4 };	// N.....Z. : d        : (d)--
    instructions[0x8C] = (SPC700InstructionEntry) { fs8C_DEC, 3, 5 };	// N.....Z. : !a       : (a)--
    instructions[0x8D] = (SPC700InstructionEntry) { fs8D_MOV, 2, 2 };	// N.....Z. : Y, #i    : Y = i
    instructions[0x8E] = (SPC700InstructionEntry) { fs8E_POP, 1, 4 };	// NVPBHIZC : PSW      : Flags = (++SP)
    instructions[0x8F] = (SPC700InstructionEntry) { fs8F_MOV, 3, 5 };	// ........ : d, #i    : (d) = i        (read)

    instructions[0x90] = (SPC700InstructionEntry) { fs90_BCC, 2, 2 };	// ........ : r        : PC+=r  if C == 0
    instructions[0x91] = (SPC700InstructionEntry) { fs91_TCALL, 1, 8 };	// ........ : 9        : CALL [$FFCC]
    instructions[0x92] = (SPC700InstructionEntry) { fs92_CLR1, 2, 4 };	// ........ : d.4      : d.4 = 0
    instructions[0x93] = (SPC700InstructionEntry) { fs93_BBC, 3, 5 };	// ........ : d.4, r   : PC+=r  if d.4 == 0
    instructions[0x94] = (SPC700InstructionEntry) { fs94_ADC, 2, 4 };	// NV..H.ZC : A, d+X   : A = A+(d+X)+C
    instructions[0x95] = (SPC700InstructionEntry) { fs95_ADC, 3, 5 };	// NV..H.ZC : A, !a+X  : A = A+(a+X)+C
    instructions[0x96] = (SPC700InstructionEntry) { fs96_ADC, 3, 5 };	// NV..H.ZC : A, !a+Y  : A = A+(a+Y)+C
    instructions[0x97] = (SPC700InstructionEntry) { fs97_ADC, 2, 6 };	// NV..H.ZC : A, [d]+Y : A = A+([d]+Y)+C
    instructions[0x98] = (SPC700InstructionEntry) { fs98_ADC, 3, 5 };	// NV..H.ZC : d, #i    : (d) = (d)+i+C
    instructions[0x99] = (SPC700InstructionEntry) { fs99_ADC, 1, 5 };	// NV..H.ZC : (X), (Y) : (X) = (X)+(Y)+C
    instructions[0x9A] = (SPC700InstructionEntry) { fs9A_SUBW, 2, 5 };	// NV..H.ZC : YA, d    : YA  = YA - (d), H on high byte
    instructions[0x9B] = (SPC700InstructionEntry) { fs9B_DEC, 2, 5 };	// N.....Z. : d+X      : (d+X)--
    instructions[0x9C] = (SPC700InstructionEntry) { fs9C_DEC, 1, 2 };	// N.....Z. : A        : A--
    instructions[0x9D] = (SPC700InstructionEntry) { fs9D_MOV, 1, 2 };	// N.....Z. : X, SP    : X = SP
    instructions[0x9E] = (SPC700InstructionEntry) { fs9E_DIV, 1, 1 };	// NV..H.Z. : YA, X    : A=YA/X, Y=mod(YA,X)
    instructions[0x9F] = (SPC700InstructionEntry) { fs9F_XCN, 1, 5 };	// N.....Z. : A        : A = (A>>4) | (A<<4)

    instructions[0xA0] = (SPC700InstructionEntry) { fsA0_EI, 1, 3 };	// .....1.. :  : I = 1
    instructions[0xA1] = (SPC700InstructionEntry) { fsA1_TCALL, 1, 8 };	// ........ : 10       : CALL [$FFCA]
    instructions[0xA2] = (SPC700InstructionEntry) { fsA2_SET1, 2, 4 };	// ........ : d.5      : d.5 = 1
    instructions[0xA3] = (SPC700InstructionEntry) { fsA3_BBS, 3, 5 };	// ........ : d.5, r   : PC+=r  if d.5 == 1
    instructions[0xA4] = (SPC700InstructionEntry) { fsA4_SBC, 2, 3 };	// NV..H.ZC : A, d     : A = A-(d)-!C
    instructions[0xA5] = (SPC700InstructionEntry) { fsA5_SBC, 3, 4 };	// NV..H.ZC : A, !a    : A = A-(a)-!C
    instructions[0xA6] = (SPC700InstructionEntry) { fsA6_SBC, 1, 3 };	// NV..H.ZC : A, (X)   : A = A-(X)-!C
    instructions[0xA7] = (SPC700InstructionEntry) { fsA7_SBC, 2, 6 };	// NV..H.ZC : A, [d+X] : A = A-([d+X])-!C
    instructions[0xA8] = (SPC700InstructionEntry) { fsA8_SBC, 2, 2 };	// NV..H.ZC : A, #i    : A = A-i-!C
    instructions[0xA9] = (SPC700InstructionEntry) { fsA9_SBC, 3, 6 };	// NV..H.ZC : dd, ds   : (dd) = (dd)-(ds)-!C
    instructions[0xAA] = (SPC700InstructionEntry) { fsAA_MOV1, 3, 4 };	// .......C : C, m.b   : C = (m.b)
    instructions[0xAB] = (SPC700InstructionEntry) { fsAB_INC, 2, 4 };	// N.....Z. : d        : (d)++
    instructions[0xAC] = (SPC700InstructionEntry) { fsAC_INC, 3, 5 };	// N.....Z. : !a       : (a)++
    instructions[0xAD] = (SPC700InstructionEntry) { fsAD_CMP, 2, 2 };	// N.....ZC : Y, #i    : Y - i
    instructions[0xAE] = (SPC700InstructionEntry) { fsAE_POP, 1, 4 };	// ........ : A        : A = (++SP)
    instructions[0xAF] = (SPC700InstructionEntry) { fsAF_MOV, 1, 4 };	// ........ : (X)+, A  : (X++) = A      (no read)

    instructions[0xB0] = (SPC700InstructionEntry) { fsB0_BCS, 2, 2 };	// ........ : r        : PC+=r  if C == 1
    instructions[0xB1] = (SPC700InstructionEntry) { fsB1_TCALL, 1, 8 };	// ........ : 11       : CALL [$FFC8]
    instructions[0xB2] = (SPC700InstructionEntry) { fsB2_CLR1, 2, 4 };	// ........ : d.5      : d.5 = 0
    instructions[0xB3] = (SPC700InstructionEntry) { fsB3_BBC, 3, 5 };	// ........ : d.5, r   : PC+=r  if d.5 == 0
    instructions[0xB4] = (SPC700InstructionEntry) { fsB4_SBC, 2, 4 };	// NV..H.ZC : A, d+X   : A = A-(d+X)-!C
    instructions[0xB5] = (SPC700InstructionEntry) { fsB5_SBC, 3, 5 };	// NV..H.ZC : A, !a+X  : A = A-(a+X)-!C
    instructions[0xB6] = (SPC700InstructionEntry) { fsB6_SBC, 3, 5 };	// NV..H.ZC : A, !a+Y  : A = A-(a+Y)-!C
    instructions[0xB7] = (SPC700InstructionEntry) { fsB7_SBC, 2, 6 };	// NV..H.ZC : A, [d]+Y : A = A-([d]+Y)-!C
    instructions[0xB8] = (SPC700InstructionEntry) { fsB8_SBC, 3, 5 };	// NV..H.ZC : d, #i    : (d) = (d)-i-!C
    instructions[0xB9] = (SPC700InstructionEntry) { fsB9_SBC, 1, 5 };	// NV..H.ZC : (X), (Y) : (X) = (X)-(Y)-!C
    instructions[0xBA] = (SPC700InstructionEntry) { fsBA_MOVW, 2, 5 };	// N.....Z. : YA, d    : YA = word (d)
    instructions[0xBB] = (SPC700InstructionEntry) { fsBB_INC, 2, 5 };	// N.....Z. : d+X      : (d+X)++
    instructions[0xBC] = (SPC700InstructionEntry) { fsBC_INC, 1, 2 };	// N.....Z. : A        : A++
    instructions[0xBD] = (SPC700InstructionEntry) { fsBD_MOV, 1, 2 };	// ........ : SP, X    : SP = X
    instructions[0xBE] = (SPC700InstructionEntry) { fsBE_DAS, 1, 3 };	// N.....ZC : A        : decimal adjust for subtraction
    instructions[0xBF] = (SPC700InstructionEntry) { fsBF_MOV, 1, 4 };	// N.....Z. : A, (X)+  : A = (X++)

    instructions[0xC0] = (SPC700InstructionEntry) { fsC0_DI, 1, 3 };	// .....0.. :  : I = 0
    instructions[0xC1] = (SPC700InstructionEntry) { fsC1_TCALL, 1, 8 };	// ........ : 12       : CALL [$FFC6]
    instructions[0xC2] = (SPC700InstructionEntry) { fsC2_SET1, 2, 4 };	// ........ : d.6      : d.6 = 1
    instructions[0xC3] = (SPC700InstructionEntry) { fsC3_BBS, 3, 5 };	// ........ : d.6, r   : PC+=r  if d.6 == 1
    instructions[0xC4] = (SPC700InstructionEntry) { fsC4_MOV, 2, 4 };	// ........ : d, A     : (d) = A        (read)
    instructions[0xC5] = (SPC700InstructionEntry) { fsC5_MOV, 3, 5 };	// ........ : !a, A    : (a) = A        (read)
    instructions[0xC6] = (SPC700InstructionEntry) { fsC6_MOV, 1, 4 };	// ........ : (X), A   : (X) = A        (read)
    instructions[0xC7] = (SPC700InstructionEntry) { fsC7_MOV, 2, 7 };	// ........ : [d+X], A : ([d+X]) = A    (read)
    instructions[0xC8] = (SPC700InstructionEntry) { fsC8_CMP, 2, 2 };	// N.....ZC : X, #i    : X - i
    instructions[0xC9] = (SPC700InstructionEntry) { fsC9_MOV, 3, 5 };	// ........ : !a, X    : (a) = X        (read)
    instructions[0xCA] = (SPC700InstructionEntry) { fsCA_MOV1, 3, 6 };	// ........ : m.b, C   : (m.b) = C
    instructions[0xCB] = (SPC700InstructionEntry) { fsCB_MOV, 2, 4 };	// ........ : d, Y     : (d) = Y        (read)
    instructions[0xCC] = (SPC700InstructionEntry) { fsCC_MOV, 3, 5 };	// ........ : !a, Y    : (a) = Y        (read)
    instructions[0xCD] = (SPC700InstructionEntry) { fsCD_MOV, 2, 2 };	// N.....Z. : X, #i    : X = i
    instructions[0xCE] = (SPC700InstructionEntry) { fsCE_POP, 1, 4 };	// ........ : X        : X = (++SP)
    instructions[0xCF] = (SPC700InstructionEntry) { fsCF_MUL, 1, 9 };	// N.....Z. : YA       : YA = Y * A, NZ on Y only

    instructions[0xD0] = (SPC700InstructionEntry) { fsD0_BNE, 2, 2 };	// ........ : r        : PC+=r  if Z == 0
    instructions[0xD1] = (SPC700InstructionEntry) { fsD1_TCALL, 1, 8 };	// ........ : 13       : CALL [$FFC4]
    instructions[0xD2] = (SPC700InstructionEntry) { fsD2_CLR1, 2, 4 };	// ........ : d.6      : d.6 = 0
    instructions[0xD3] = (SPC700InstructionEntry) { fsD3_BBC, 3, 5 };	// ........ : d.6, r   : PC+=r  if d.6 == 0
    instructions[0xD4] = (SPC700InstructionEntry) { fsD4_MOV, 2, 5 };	// ........ : d+X, A   : (d+X) = A      (read)
    instructions[0xD5] = (SPC700InstructionEntry) { fsD5_MOV, 3, 6 };	// ........ : !a+X, A  : (a+X) = A      (read)
    instructions[0xD6] = (SPC700InstructionEntry) { fsD6_MOV, 3, 6 };	// ........ : !a+Y, A  : (a+Y) = A      (read)
    instructions[0xD7] = (SPC700InstructionEntry) { fsD7_MOV, 2, 7 };	// ........ : [d]+Y, A : ([d]+Y) = A    (read)
    instructions[0xD8] = (SPC700InstructionEntry) { fsD8_MOV, 2, 4 };	// ........ : d, X     : (d) = X        (read)
    instructions[0xD9] = (SPC700InstructionEntry) { fsD9_MOV, 2, 5 };	// ........ : d+Y, X   : (d+Y) = X      (read)
    instructions[0xDA] = (SPC700InstructionEntry) { fsDA_MOVW, 2, 5 };	// ........ : d, YA    : word (d) = YA  (read low only)
    instructions[0xDB] = (SPC700InstructionEntry) { fsDB_MOV, 2, 5 };	// ........ : d+X, Y   : (d+X) = Y      (read)
    instructions[0xDC] = (SPC700InstructionEntry) { fsDC_DEC, 1, 2 };	// N.....Z. : Y        : Y--
    instructions[0xDD] = (SPC700InstructionEntry) { fsDD_MOV, 1, 2 };	// N.....Z. : A, Y     : A = Y
    instructions[0xDE] = (SPC700InstructionEntry) { fsDE_CBNE, 3, 6 };	// ........ : d+X, r   : CMP A, (d+X) then BNE
    instructions[0xDF] = (SPC700InstructionEntry) { fsDF_DAA, 1, 3 };	// N.....ZC : A        : decimal adjust for addition

    instructions[0xE0] = (SPC700InstructionEntry) { fsE0_CLRV, 1, 2 };	// .0..0... :  : V = 0, H = 0
    instructions[0xE1] = (SPC700InstructionEntry) { fsE1_TCALL, 1, 8 };	// ........ : 14       : CALL [$FFC2]
    instructions[0xE2] = (SPC700InstructionEntry) { fsE2_SET1, 2, 4 };	// ........ : d.7      : d.7 = 1
    instructions[0xE3] = (SPC700InstructionEntry) { fsE3_BBS, 3, 5 };	// ........ : d.7, r   : PC+=r  if d.7 == 1
    instructions[0xE4] = (SPC700InstructionEntry) { fsE4_MOV, 2, 3 };	// N.....Z. : A, d     : A = (d)
    instructions[0xE5] = (SPC700InstructionEntry) { fsE5_MOV, 3, 4 };	// N.....Z. : A, !a    : A = (a)
    instructions[0xE6] = (SPC700InstructionEntry) { fsE6_MOV, 1, 3 };	// N.....Z. : A, (X)   : A = (X)
    instructions[0xE7] = (SPC700InstructionEntry) { fsE7_MOV, 2, 6 };	// N.....Z. : A, [d+X] : A = ([d+X])
    instructions[0xE8] = (SPC700InstructionEntry) { fsE8_MOV, 2, 2 };	// N.....Z. : A, #i    : A = i
    instructions[0xE9] = (SPC700InstructionEntry) { fsE9_MOV, 3, 4 };	// N.....Z. : X, !a    : X = (a)
    instructions[0xEA] = (SPC700InstructionEntry) { fsEA_NOT1, 3, 5 };	// ........ : m.b      : m.b = ~m.b
    instructions[0xEB] = (SPC700InstructionEntry) { fsEB_MOV, 2, 3 };	// N.....Z. : Y, d     : Y = (d)
    instructions[0xEC] = (SPC700InstructionEntry) { fsEC_MOV, 3, 4 };	// N.....Z. : Y, !a    : Y = (a)
    instructions[0xED] = (SPC700InstructionEntry) { fsED_NOTC, 1, 3 };	// .......C :  : C = !C
    instructions[0xEE] = (SPC700InstructionEntry) { fsEE_POP, 1, 4 };	// ........ : Y        : Y = (++SP)
    instructions[0xEF] = (SPC700InstructionEntry) { fsEF_SLEEP, 1, 0 };	// ........ :  : Halts the processor

    instructions[0xF0] = (SPC700InstructionEntry) { fsF0_BEQ, 2, 2 };	// ........ : r        : PC+=r  if Z == 1
    instructions[0xF1] = (SPC700InstructionEntry) { fsF1_TCALL, 1, 8 };	// ........ : 15       : CALL [$FFC0]
    instructions[0xF2] = (SPC700InstructionEntry) { fsF2_CLR1, 2, 4 };	// ........ : d.7      : d.7 = 0
    instructions[0xF3] = (SPC700InstructionEntry) { fsF3_BBC, 3, 5 };	// ........ : d.7, r   : PC+=r  if d.7 == 0
    instructions[0xF4] = (SPC700InstructionEntry) { fsF4_MOV, 2, 4 };	// N.....Z. : A, d+X   : A = (d+X)
    instructions[0xF5] = (SPC700InstructionEntry) { fsF5_MOV, 3, 5 };	// N.....Z. : A, !a+X  : A = (a+X)
    instructions[0xF6] = (SPC700InstructionEntry) { fsF6_MOV, 3, 5 };	// N.....Z. : A, !a+Y  : A = (a+Y)
    instructions[0xF7] = (SPC700InstructionEntry) { fsF7_MOV, 2, 6 };	// N.....Z. : A, [d]+Y : A = ([d]+Y)
    instructions[0xF8] = (SPC700InstructionEntry) { fsF8_MOV, 2, 3 };	// N.....Z. : X, d     : X = (d)
    instructions[0xF9] = (SPC700InstructionEntry) { fsF9_MOV, 2, 4 };	// N.....Z. : X, d+Y   : X = (d+Y)
    instructions[0xFA] = (SPC700InstructionEntry) { fsFA_MOV, 3, 5 };	// ........ : dd, ds   : (dd) = (ds)    (no read)
    instructions[0xFB] = (SPC700InstructionEntry) { fsFB_MOV, 2, 4 };	// N.....Z. : Y, d+X   : Y = (d+X)
    instructions[0xFC] = (SPC700InstructionEntry) { fsFC_INC, 1, 2 };	// N.....Z. : Y        : Y++
    instructions[0xFD] = (SPC700InstructionEntry) { fsFD_MOV, 1, 2 };	// N.....Z. : Y, A     : Y = A
    instructions[0xFE] = (SPC700InstructionEntry) { fsFE_DBNZ, 2, 4 };	// ........ : Y, r     : Y-- then JNZ
    instructions[0xFF] = (SPC700InstructionEntry) { fsFF_STOP, 1, 0 };	// ........ :  : Halts the processor 
}

#pragma endregion