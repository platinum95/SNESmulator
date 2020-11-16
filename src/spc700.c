
#include "spc700.h"
#include "spc700_instruction_declarations.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "system.h"

/*

    Emulation of the SPC700 sound chip
    Aint no simple chip herey boy.

*/

/*
Basic memory map
0x0000 - 0x00FF - direct page 0
0x00F0 - 0x00FF - memory - mapped hardware registers
0x0100 - 0x01FF - direct page 1
0x0100 - 0x01FF - potential stack memory
0xFFC0 - 0xFFFF - IPL ROM
*/

void(*spc700_instructions[16 * 16])();

typedef struct Registers {
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
static uint16_t program_counter;
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
static uint16_t YA() {
    uint16_t toRet = Y;
    toRet <<= 8;
    toRet |= A;
    return toRet;
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

uint8_t *accessPageAddr( uint8_t addr )
{
    return spc_memory_map( ( ( PSW & SPC_P_FLAG ) ? 0x0100 : 0x0000 ) + addr );
}

/* Access the 4 visible bytes from the CPU */
uint8_t *access_spc_snes_mapped(uint16_t addr) {
    addr = addr - 0x2140 + 0x00f4;
    if (addr < 0x00f4 || addr > 0x00f7)
        return NULL;
    else
        return &spc_memory[addr];
}

/* Execute next instruction, update PC and cycle counter etc */
void spc700_execute_next_instruction() {
    void(*next_instruction)() = *spc700_instructions[0x00];
    next_instruction();
}

#pragma region SPC_ADDRESSING_MODES
static uint8_t *immediate() {
    return &spc_memory[ program_counter ];
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
    return *direct( 0 );
}
static uint16_t direct_16( uint8_t offset ) {
    return get2Byte( direct( offset ) );
}
static uint8_t direct_indexed_X_8() {
    return *direct( X );
}
static uint16_t direct_indexed_X_16() {
    return get2Byte( direct( X ) );
}
static uint8_t direct_indexed_Y_8() {
    return *direct( Y );
}
static uint16_t direct_indexed_Y_16() {
    return get2Byte( direct( Y ) );
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
    return get2Byte( immediate() );
}
static uint8_t *absolute( uint8_t offset ) {
    return spc_memory_map( absolute_addr() + offset );
}
static uint8_t absolute_8() {
    return *absolute( 0 );
}
static uint16_t absolute_16() {
    return get2Byte( absolute( 0 ) );
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

// TODO - switch to 1 and 2 param ops
// TODO - increment PC as data consumed (fix param ordering)

// DIRECT OPS START
#define DIRECT_D_OP( op ) \
    op( direct() );

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

#define DIRECT_D_YA_OP( op ) \
    uint16_t YA; /* TODO - get YA */ \
    uint16_t o; \
    op( &o, YA ); \
    /* TODO - set D word */ \

#define DIRECT_YA_D_OP( op ) \
    uint16_t YA; \
    op( &YA, direct( 0 ) ); \
    /* TODO - get YA/set Y and A */ \
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
    uint8_t nearLabel = 0x00; /* TODO - get nearLabel */ \
    op( direct( X ), &nearLabel );
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
// TODO - verify inc/dec ordering
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
    ABSOLUTE_BOOLEAN_BIT_BASE( op, loc, bMask, (*loc) & bMask );
    
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
#define ABSOLUTE_X_INDEXED_INDIRECT( op )\
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
    op( program_counter + immediate_8() ); // TODO - verify this

#define RELATIVE_OP_D_R( op ) \
    op( direct(), immediate() ); // TODO - ordering

#define RELATIVE_OP_Y_R( op ) \
    op( &Y, immediate() ); // TODO - ordering
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
    /* TODO - set YA */

#define IMPLIED_YA_X_OP( op ) \
    uint16_t YA; \
    op( &YA, &X ); \
    /* TODO - set YA */
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

static void ADDW( uint16_t *O1, uint8_t *O2 ) {
    // TODO
}

static void SBC( uint8_t *A, uint8_t *B ) {
    uint8_t Bneg = ( ~( *B + ( PSW & SPC_CARRY_FLAG ? 0 : 1 ) ) ) + 1;
    ADC( A, &Bneg );
}

static void SUBW( uint16_t *O1, uint8_t *O2 ) {
    // TODO
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
    DIRECT_YA_D_OP( ADDW );
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
    DIRECT_YA_D_OP( SUBW );
}

#pragma endregion 

#pragma region SPC_AND
static void AND( uint8_t *O1, uint8_t *O2 ) {
    *O1 = *O1 & *O2;

    if ( *O1 == 0 )
    {
        PSW |= SPC_ZERO_FLAG;
    }
    else if ( *O1 & 0x80 )
    {
        PSW |= SPC_NEGATIVE_FLAG;
    }
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
static void LSR(uint8_t *B) {

}
static void ASL(uint8_t B) {
}

static void fs1C_ASL() {
}
static void fs0B_ASL() {
}
static void fs1B_ASL() {
}
static void fs0C_ASL() {
}

static void fs5C_LSR() {
}
static void fs4B_LSR() {
}
static void fs4C_LSR() {
}
static void fsAF_LSR() {
}

static void fs3C_ROL() {
}
static void fs2B_ROL() {
}
static void fs3B_ROL() {
}
static void fs2C_ROL() {
}
static void fs7C_ROR() {
}
static void fs6B_ROR() {
}
static void fs7B_ROR() {
}
static void fs6C_ROR() {
}

#pragma endregion

#pragma region SPC_BRANCH
static void BBC( bool O1, uint8_t* r ) {
    // TODO 
}

static void BBS( bool O1, uint8_t* r ) {
    // TODO 
}

static void JMP( uint16_t addr )
{
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
    PSW |= SPC_BREAK_FLAG;
    //PUSH( program_counter )
    //PUSH( PSW )
    PSW &= ~SPC_INTERRUPT_FLAG;
    JMP( 0xFFDE );
}

static void fs1F_JMP() {
    ABSOLUTE_a_addr_OP( JMP );
}
static void fs5F_JMP() {
    // TODO - verify
    ABSOLUTE_X_INDEXED_INDIRECT( JMP );
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

static void fs12_CLRC() {
    CLR( &PSW, SPC_CARRY_FLAG );
}
static void fs12_CLRP() {
    CLR( &PSW, SPC_P_FLAG );
}
static void fs12_CLRB() {
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
static void fs1A_DEC() {
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
static void MOV( uint8_t *O1, uint8_t *O2 ) {
    // TODO
    *O1 = *O2;
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
    IMMEDIATE_A_I_OP( MOV );
}
static void fsE6_MOV() {
    INDIRECT_A_X_OP( MOV );
}
static void fsBF_MOV() {
    INDIRECT_AUTO_INC_A_X_OP( MOV );
}
static void fsF7_MOV() {
   INDIRECT_Y_INDEXED_A_dY_OP( MOV );
}
static void fsE7_MOV() {
    X_INDEXED_INDIRECT_A_dX_OP( MOV );
}
static void fs7D_MOV() {
}
static void fsDD_MOV() {
}
static void fsE4_MOV() {
    DIRECT_A_D_OP( MOV );
}
static void fsF4_MOV() {
}
static void fsE5_MOV() {
}
static void fsF5_MOV() {
}
static void fsF6_MOV() {
}
static void fsBD_MOV() {
}
static void fsCD_MOV() {
    IMMEDIATE_X_I_OP( MOV );
}
static void fs5D_MOV() {
}
static void fs9D_MOV() {
}
static void fsF8_MOV() {
}
static void fsF9_MOV() {
}
static void fsE9_MOV() {
}
static void fs8D_MOV() {
    IMMEDIATE_Y_I_OP( MOV );
}
static void fsFD_MOV() {
}
static void fsEB_MOV() {
}
static void fsFB_MOV() {
}
static void fsEC_MOV() {
}
static void fsFA_MOV() {
}
static void fsD4_MOV() {
}
static void fsDB_MOV() {
}
static void fsD9_MOV() {
}
static void fs8F_MOV() {
}
static void fsC4_MOV() {
    DIRECT_D_A_OP( MOV );
}
static void fsD8_MOV() {
}
static void fsCB_MOV() {
}
static void fsD5_MOV() {
}
static void fsD6_MOV() {
}
static void fsC5_MOV() {
}
static void fsC9_MOV() {
}
static void fsCC_MOV() {
}
static void fsAA_MOV1() {
}
static void fsCA_MOV1() {
}
static void fsBA_MOV1() {
}
static void fsDA_MOVW() {
}

#pragma endregion

#pragma region SPC_STACK
static void POP( uint8_t *O1 ) {

}
static void fsAE_POP() {
}
static void fs8E_POP() {
}
static void fsCE_POP() {
}
static void fsEE_POP() {
}
static void fs2D_PUSH() {
}
static void fs0D_PUSH() {
}
static void fs4D_PUSH() {
}
static void fs6D_PUSH() {
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
static void fs43_SET1() {
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
static void fs3F_CALL() {
}
static void fsDE_CBNE() {
}
static void fs2E_CBNE() {
}
static void fsDF_DAA() {
}
static void fsBE_DAS() {
}
static void fsFE_DBNZ() {
}
static void fs6E_DBNZ() {
}
static void fs9E_DIV() {
}
static void fsC0_DI() {
    CLR( &PSW, SPC_INTERRUPT_FLAG );
}
static void fsA0_EI() {
    SET( &PSW, SPC_INTERRUPT_FLAG );
}
static void fsCF_MUL() {
}

static void fs00_NOP() {
}

static void fsEA_NOT1() {
}
static void fsED_NOTC() {
}

static void fs4F_PCALL() {
}
static void fs6F_RET() {
    program_counter = stack_pop_8();
}
static void fs7F_RETI() {
}
static void fsEF_SLEEP() {
}
static void fsFF_STOP() {
}

static void fs4E_TCLR1() {
    uint8_t *a = absolute( 0 );
    uint16_t a_8 = *a;
    a_8 = a_8 & (~A);
    *a = a_8;
    uint8_t res = A - a_8;
    spc_set_PSW_register(res, SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG);
    program_counter += 3;
}
static void fs0E_TSET1() {
    uint8_t *a = absolute( 0 );
    uint16_t a_8 = *a;
    a_8 = (a_8 | A);
    *a = a_8;
    uint8_t res = A - a_8;
    spc_set_PSW_register(res, SPC_ZERO_FLAG | SPC_NEGATIVE_FLAG);
    program_counter += 3;
}
static void fs9F_XCN() {
    A = (A >> 4) | (A << 4);
    program_counter += 1;
}

#pragma endregion
/* Populate the instruction functions array */
void spc700_populate_instructions() {
    spc700_instructions[0x00] = fs00_NOP;
    spc700_instructions[0x01] = fs01_TCALL;
    spc700_instructions[0x02] = fs02_SET1;
    spc700_instructions[0x03] = fs03_BBS;
    spc700_instructions[0x04] = fs04_OR;
    spc700_instructions[0x05] = fs05_OR;
    spc700_instructions[0x06] = fs06_OR;
    spc700_instructions[0x07] = fs07_OR;
    spc700_instructions[0x08] = fs08_OR;
    spc700_instructions[0x09] = fs09_OR;
    spc700_instructions[0x0A] = fs0A_OR1;
    spc700_instructions[0x0B] = fs0B_ASL;
    spc700_instructions[0x0C] = fs0C_ASL;
    spc700_instructions[0x0D] = fs0D_PUSH;
    spc700_instructions[0x0E] = fs0E_TSET1;
    spc700_instructions[0x0F] = fs0F_BRK;
    spc700_instructions[0x10] = fs10_BPL;

    spc700_instructions[0x11] = fs11_TCALL;
    spc700_instructions[0x12] = fs12_CLRB;
    spc700_instructions[0x13] = fs13_BBC;
    spc700_instructions[0x14] = fs14_OR;
    spc700_instructions[0x15] = fs15_OR;
    spc700_instructions[0x16] = fs16_OR;
    spc700_instructions[0x17] = fs17_OR;
    spc700_instructions[0x18] = fs18_OR;
    spc700_instructions[0x19] = fs19_OR;
    spc700_instructions[0x1A] = fs1A_DEC;
    spc700_instructions[0x1B] = fs1B_ASL;
    spc700_instructions[0x1C] = fs1C_ASL;
    spc700_instructions[0x1D] = fs1D_DEC;
    spc700_instructions[0x1E] = fs1E_CMP;
    spc700_instructions[0x1F] = fs1F_JMP;

    spc700_instructions[0x21] = fs21_TCALL;
    spc700_instructions[0x22] = fs22_SET1;
    spc700_instructions[0x23] = fs23_BBS;
    spc700_instructions[0x24] = fs24_AND;
    spc700_instructions[0x25] = fs25_AND;
    spc700_instructions[0x26] = fs26_AND;
    spc700_instructions[0x27] = fs27_AND;
    spc700_instructions[0x28] = fs28_AND;
    spc700_instructions[0x29] = fs29_AND;
    spc700_instructions[0x2A] = fs2A_OR1;
    spc700_instructions[0x2B] = fs2B_ROL;
    spc700_instructions[0x2C] = fs2C_ROL;
    spc700_instructions[0x2D] = fs2D_PUSH;
    spc700_instructions[0x2E] = fs2E_CBNE;
    spc700_instructions[0x2F] = fs2F_BRA;
    spc700_instructions[0x30] = fs30_BMI;

    spc700_instructions[0x31] = fs31_TCALL;
    spc700_instructions[0x32] = fs32_CLR1;
    spc700_instructions[0x33] = fs33_BBC;
    spc700_instructions[0x34] = fs34_AND;
    spc700_instructions[0x35] = fs35_AND;
    spc700_instructions[0x36] = fs36_AND;
    spc700_instructions[0x37] = fs37_AND;
    spc700_instructions[0x38] = fs38_AND;
    spc700_instructions[0x39] = fs39_AND;
    spc700_instructions[0x3A] = fs3A_INCW;
    spc700_instructions[0x3B] = fs3B_ROL;
    spc700_instructions[0x3C] = fs3C_ROL;
    spc700_instructions[0x3D] = fs3D_INC;
    spc700_instructions[0x3E] = fs3E_CMP;
    spc700_instructions[0x3F] = fs3F_CALL;
    spc700_instructions[0x40] = fs40_SETP;

    spc700_instructions[0x41] = fs41_TCALL;
    spc700_instructions[0x43] = fs43_SET1;
    spc700_instructions[0x44] = fs44_EOR;
    spc700_instructions[0x45] = fs45_EOR;
    spc700_instructions[0x46] = fs46_EOR;
    spc700_instructions[0x47] = fs47_EOR;
    spc700_instructions[0x48] = fs48_EOR;
    spc700_instructions[0x49] = fs49_EOR;
    spc700_instructions[0x4A] = fs4A_AND1;
    spc700_instructions[0x4B] = fs4B_LSR;
    spc700_instructions[0x4C] = fs4C_LSR;
    spc700_instructions[0x4D] = fs4D_PUSH;
    spc700_instructions[0x4E] = fs4E_TCLR1;
    spc700_instructions[0x4F] = fs4F_PCALL;
    spc700_instructions[0x50] = fs50_BVC;

    spc700_instructions[0x51] = fs51_TCALL;
    spc700_instructions[0x52] = fs52_CLR1;
    spc700_instructions[0x53] = fs53_BBC;
    spc700_instructions[0x54] = fs54_EOR;
    spc700_instructions[0x55] = fs55_EOR;
    spc700_instructions[0x56] = fs56_EOR;
    spc700_instructions[0x57] = fs57_EOR;
    spc700_instructions[0x58] = fs58_EOR;
    spc700_instructions[0x59] = fs59_EOR;
    spc700_instructions[0x5A] = fs5A_CMPW;
    spc700_instructions[0x5C] = fs5C_LSR;
    spc700_instructions[0x5D] = fs5D_MOV;
    spc700_instructions[0x5E] = fs5E_CMP;
    spc700_instructions[0x5F] = fs5F_JMP;

    spc700_instructions[0x61] = fs61_TCALL;
    spc700_instructions[0x62] = fs62_SET1;
    spc700_instructions[0x63] = fs63_BBS;
    spc700_instructions[0x64] = fs64_CMP;
    spc700_instructions[0x65] = fs65_CMP;
    spc700_instructions[0x66] = fs66_CMP;
    spc700_instructions[0x67] = fs67_CMP;
    spc700_instructions[0x68] = fs68_CMP;
    spc700_instructions[0x69] = fs69_CMP;
    spc700_instructions[0x6A] = fs6A_AND1;
    spc700_instructions[0x6B] = fs6B_ROR;
    spc700_instructions[0x6C] = fs6C_ROR;
    spc700_instructions[0x6D] = fs6D_PUSH;
    spc700_instructions[0x6E] = fs6E_DBNZ;
    spc700_instructions[0x6F] = fs6F_RET;
    spc700_instructions[0x70] = fs70_BVS;

    spc700_instructions[0x71] = fs71_TCALL;
    spc700_instructions[0x72] = fs72_CLR1;
    spc700_instructions[0x73] = fs73_BBC;
    spc700_instructions[0x74] = fs74_CMP;
    spc700_instructions[0x75] = fs75_CMP;
    spc700_instructions[0x76] = fs76_CMP;
    spc700_instructions[0x77] = fs77_CMP;
    spc700_instructions[0x78] = fs78_CMP;
    spc700_instructions[0x79] = fs79_CMP;
    spc700_instructions[0x7A] = fs7A_ADDW;
    spc700_instructions[0x7B] = fs7B_ROR;
    spc700_instructions[0x7C] = fs7C_ROR;
    spc700_instructions[0x7D] = fs7D_MOV;
    spc700_instructions[0x7E] = fs7E_CMP;
    spc700_instructions[0x7F] = fs7F_RETI;
    spc700_instructions[0x80] = fs80_SETC;

    spc700_instructions[0x81] = fs81_TCALL;
    spc700_instructions[0x82] = fs82_SET1;
    spc700_instructions[0x83] = fs83_BBS;
    spc700_instructions[0x84] = fs84_ADC;
    spc700_instructions[0x85] = fs85_ADC;
    spc700_instructions[0x86] = fs86_ADC;
    spc700_instructions[0x87] = fs87_ADC;
    spc700_instructions[0x88] = fs88_ADC;
    spc700_instructions[0x89] = fs89_ADC;
    spc700_instructions[0x8A] = fs8A_EOR1;
    spc700_instructions[0x8B] = fs8B_DEC;
    spc700_instructions[0x8C] = fs8C_DEC;
    spc700_instructions[0x8D] = fs8D_MOV;
    spc700_instructions[0x8E] = fs8E_POP;
    spc700_instructions[0x8F] = fs8F_MOV;
    spc700_instructions[0x90] = fs90_BCC;
    spc700_instructions[0x90] = fs91_TCALL;

    spc700_instructions[0x92] = fs92_CLR1;
    spc700_instructions[0x93] = fs93_BBC;
    spc700_instructions[0x94] = fs94_ADC;
    spc700_instructions[0x95] = fs95_ADC;
    spc700_instructions[0x96] = fs96_ADC;
    spc700_instructions[0x97] = fs97_ADC;
    spc700_instructions[0x98] = fs98_ADC;
    spc700_instructions[0x99] = fs99_ADC;
    spc700_instructions[0x9A] = fs9A_SUBW;
    spc700_instructions[0x9B] = fs9B_DEC;
    spc700_instructions[0x9C] = fs9C_DEC;
    spc700_instructions[0x9D] = fs9D_MOV;
    spc700_instructions[0x9E] = fs9E_DIV;
    spc700_instructions[0x9F] = fs9F_XCN;
    spc700_instructions[0xA0] = fsA0_EI;

    spc700_instructions[0xA1] = fsA1_TCALL;
    spc700_instructions[0xA2] = fsA2_SET1;
    spc700_instructions[0xA3] = fsA3_BBS;
    spc700_instructions[0xA4] = fsA4_SBC;
    spc700_instructions[0xA5] = fsA5_SBC;
    spc700_instructions[0xA6] = fsA6_SBC;
    spc700_instructions[0xA7] = fsA7_SBC;
    spc700_instructions[0xA8] = fsA8_SBC;
    spc700_instructions[0xA9] = fsA9_SBC;
    spc700_instructions[0xAA] = fsAA_MOV1;
    spc700_instructions[0xAB] = fsAB_INC;
    spc700_instructions[0xAC] = fsAC_INC;
    spc700_instructions[0xAD] = fsAD_CMP;
    spc700_instructions[0xAE] = fsAE_POP;
    spc700_instructions[0xAF] = fsAF_MOV;
    spc700_instructions[0xB0] = fsB0_BCS;

    spc700_instructions[0xB1] = fsB1_TCALL;
    spc700_instructions[0xB2] = fsB2_CLR1;
    spc700_instructions[0xB3] = fsB3_BBC;
    spc700_instructions[0xB4] = fsB4_SBC;
    spc700_instructions[0xB5] = fsB5_SBC;
    spc700_instructions[0xB6] = fsB6_SBC;
    spc700_instructions[0xB7] = fsB7_SBC;
    spc700_instructions[0xB8] = fsB8_SBC;
    spc700_instructions[0xB9] = fsB9_SBC;
    spc700_instructions[0xBA] = fsBA_MOV1;
    spc700_instructions[0xBB] = fsBB_INC;
    spc700_instructions[0xBC] = fsBC_INC;
    spc700_instructions[0xBD] = fsBD_MOV;
    spc700_instructions[0xBE] = fsBE_DAS;
    spc700_instructions[0xBF] = fsBF_MOV;
    spc700_instructions[0xC0] = fsC0_DI;

    spc700_instructions[0xC1] = fsC1_TCALL;
    spc700_instructions[0xC2] = fsC2_SET1;
    spc700_instructions[0xC3] = fsC3_BBS;
    spc700_instructions[0xC4] = fsC4_MOV;
    spc700_instructions[0xC5] = fsC5_MOV;
    spc700_instructions[0xC6] = fsC6_MOV;
    spc700_instructions[0xC7] = fsC7_MOV;
    spc700_instructions[0xC8] = fsC8_CMP;
    spc700_instructions[0xC9] = fsC9_MOV;
    spc700_instructions[0xCA] = fsCA_MOV1;
    spc700_instructions[0xCB] = fsCB_MOV;
    spc700_instructions[0xCC] = fsCC_MOV;
    spc700_instructions[0xCD] = fsCD_MOV;
    spc700_instructions[0xCE] = fsCE_POP;
    spc700_instructions[0xCF] = fsCF_MUL;
    spc700_instructions[0xD0] = fsD0_BNE;

    spc700_instructions[0xD1] = fsD1_TCALL;
    spc700_instructions[0xD2] = fsD2_CLR1;
    spc700_instructions[0xD3] = fsD3_BBC;
    spc700_instructions[0xD4] = fsD4_MOV;
    spc700_instructions[0xD5] = fsD5_MOV;
    spc700_instructions[0xD6] = fsD6_MOV;
    spc700_instructions[0xD7] = fsD7_MOV;
    spc700_instructions[0xD8] = fsD8_MOV;
    spc700_instructions[0xD9] = fsD9_MOV;
    spc700_instructions[0xDA] = fsDA_MOVW;
    spc700_instructions[0xDB] = fsDB_MOV;
    spc700_instructions[0xDC] = fsDC_DEC;
    spc700_instructions[0xDD] = fsDD_MOV;
    spc700_instructions[0xDE] = fsDE_CBNE;
    spc700_instructions[0xDF] = fsDF_DAA;

    spc700_instructions[0xE1] = fsE1_TCALL;
    spc700_instructions[0xE2] = fsE2_SET1;
    spc700_instructions[0xE3] = fsE3_BBS;
    spc700_instructions[0xE4] = fsE4_MOV;
    spc700_instructions[0xE5] = fsE5_MOV;
    spc700_instructions[0xE6] = fsE6_MOV;
    spc700_instructions[0xE7] = fsE7_MOV;
    spc700_instructions[0xE8] = fsE8_MOV;
    spc700_instructions[0xE9] = fsE9_MOV;
    spc700_instructions[0xEA] = fsEA_NOT1;
    spc700_instructions[0xEB] = fsEB_MOV;
    spc700_instructions[0xEC] = fsEC_MOV;
    spc700_instructions[0xED] = fsED_NOTC;
    spc700_instructions[0xEE] = fsEE_POP;
    spc700_instructions[0xEF] = fsEF_SLEEP;
    spc700_instructions[0xF0] = fsF0_BEQ;

    spc700_instructions[0xF1] = fsF1_TCALL;
    spc700_instructions[0xF2] = fsF2_CLR1;
    spc700_instructions[0xF3] = fsF3_BBC;
    spc700_instructions[0xF4] = fsF4_MOV;
    spc700_instructions[0xF5] = fsF5_MOV;
    spc700_instructions[0xF6] = fsF6_MOV;
    spc700_instructions[0xF7] = fsF7_MOV;
    spc700_instructions[0xF8] = fsF8_MOV;
    spc700_instructions[0xF9] = fsF9_MOV;
    spc700_instructions[0xFA] = fsFA_MOV;
    spc700_instructions[0xFB] = fsFB_MOV;
    spc700_instructions[0xFC] = fsFC_INC;
    spc700_instructions[0xFD] = fsFD_MOV;
    spc700_instructions[0xFE] = fsFE_DBNZ;

}

#pragma endregion