#include "spc700.h"
#include "spc700_instruction_declarations.h"
#include "system.h"
#include <stdint.h>
#include <stddef.h>

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

/* Memory and registers */
static uint8_t spc_memory[65536];
static uint16_t program_counter;
static uint8_t A, X, Y, SP, PSW;

/* Status register flags */
#define CARRY_FLAG  0x1
#define ZERO_FLAG 0x2
#define INTERRUPT_FLAG 0x04
#define HALF_CARRY_FLAG 0x08
#define BREAK_FLAG 0x10
#define P_FLAG 0x20
#define OVERFLOW_FLAG 0x40
#define NEGATIVE_FLAG 0x80

/* 16 bit "register" from A and Y registers */
static uint16_t YA() {
    uint16_t toRet = Y;
    toRet <<= 8;
    toRet |= A;
    return toRet;
}

/* 16 bit direct page = 0x0(P_FLAG)00 | DP_register */
static uint16_t get_direct_page(uint8_t dp) {
    uint16_t base = dp;
    if (PSW | P_FLAG)
        return base | 0x0100;
    return base;
}

/* 16 bit stack pointer = 0x0100 | SP_register */
static uint16_t get_stack_pointer() {
    uint16_t toRet = SP;
    return toRet | 0x01;

}

/* Initialise (power on) */
void spc700_initialise() {
    spc_memory[0x00F4] = 0xAA;
    spc_memory[0x00F5] = 0xBB;
    SP = 0xEF;
}


/* Access the SPC memory */
uint8_t *get_spc_memory() {
    return spc_memory;
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
    return &spc_memory[program_counter];
}
static uint8_t immediate_8() {
    return *immediate();
}
static uint8_t *indirect_X() {
    return &spc_memory[X];
}
static uint8_t indirect_X_8() {
    return *indirect_X();
}
static uint8_t indirect_X_AI_8() {
    uint8_t toRet = *indirect_X();
    X++;
    return toRet;
}

static uint8_t *direct(uint8_t offset) {
    uint8_t dp = immediate_8() + offset;
    uint16_t dp_addr = get_direct_page(offset);
    return &spc_memory[dp_addr];
}
static uint8_t direct_8() {
    return *direct(0);
}
static uint16_t direct_16() {
    return get2Byte(direct(0));
}
static uint8_t direct_indexed_X_8() {
    return *direct(X);
}
static uint16_t direct_indexed_X_16() {
    return get2Byte(direct(X));
}
static uint8_t direct_indexed_Y_8() {
    return *direct(Y);
}
static uint16_t direct_indexed_Y_16() {
    return get2Byte(direct(Y));
}

static uint8_t *direct_indexed_X_indirect() {
    uint16_t addr = direct_indexed_X_16();
    return &spc_memory[addr];
}
static uint8_t direct_indexed_X_indirect_8() {
    return *direct_indexed_X_indirect();
}
static uint8_t *direct_indirect_indexed_Y_indirect() {
    uint16_t addr = direct_16() + Y;
    return &spc_memory[addr];
}
static uint8_t direct_indirect_indexed_Y_indirect_8(){
    return *direct_indirect_indexed_Y_indirect();
}

static uint8_t direct_indexed_Y_indirect_8() {
    return *direct_indirect_indexed_Y_indirect();
}



static uint16_t absolute_addr() {
    return get2Byte(immediate());
}
static uint8_t *absolute() {
    return &spc_memory[absolute_addr()];
}
static uint8_t absolute_8() {
    return *absolute();
}
static uint16_t absolute_16() {
    return get2Byte(absolute());
}

static uint8_t absolute_indexed_X_8(){
    uint16_t addr = absolute_addr() + X;
    return spc_memory[addr];
}
static uint8_t absolute_indexed_Y_8() {
    uint16_t addr = absolute_addr() + Y;
    return spc_memory[addr];
}

static uint8_t stack_pop_8(){
    SP -= 1;
    return spc_memory[SP];
}


#pragma endregion

void spc_set_PSW_register(uint8_t val, uint8_t flags) {
    if (val == 0)
        PSW |= ZERO_FLAG & flags;
    else
        PSW &= ~(ZERO_FLAG & flags);

    if (val & 0x80)
        PSW |= NEGATIVE_FLAG & flags;
    else
        PSW &= ~(NEGATIVE_FLAG & flags);
}

#pragma region SPC_INSTRUCTIONS
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

#pragma region SPC_ADD_SBC
static void ADC(uint8_t *R, uint8_t A, uint8_t B) {
    uint8_t AnL = A & 0x0F, BnL = B & 0x0F;
    uint8_t Hc = AnL + BnL + (PSW & CARRY_FLAG);
    //Half carry if lower nybbles carry
    if (Hc & 0xF0) {
        PSW |= HALF_CARRY_FLAG;
    }
    else
        PSW &= ~HALF_CARRY_FLAG;

    uint16_t Rb = A + B + (PSW & CARRY_FLAG);
    if (((0x80 & A) == (0x80 & B)) && ((0x80 & Rb) != (0x80 & A))) {
        PSW |= OVERFLOW_FLAG;
    }
    else {
        PSW &= ~OVERFLOW_FLAG;
    }

    if (Rb & 0xFF00) {
        PSW |= CARRY_FLAG;
    }
    else
        PSW &= ~CARRY_FLAG;

    *R = (uint8_t)Rb;
    spc_set_PSW_register(*R, NEGATIVE_FLAG | ZERO_FLAG);
}
static void SBC(uint8_t *R, uint8_t A, uint8_t B) {
    uint8_t C = PSW & CARRY_FLAG ? 0 : 1;
    uint8_t Bneg = B + C;
    Bneg = ~Bneg;
    Bneg += 1;
    ADC(R, A, Bneg);
}
static void fs99_ADC() {
    ADC(&X, X, Y);
}
static void fs88_ADC() {
    ADC(&A, A, immediate_8());
}
static void fs86_ADC() {
    ADC(&A, A, X);
}
static void fs97_ADC() {
    uint8_t val = *direct_indirect_indexed_Y_indirect();
    ADC(&A, A, val);
}
static void fs87_ADC() {
    uint8_t dp = direct_indexed_X_indirect_8();
    ADC(&A, A, dp);
}
static void fs84_ADC() {
    uint8_t dp = direct_8();
    ADC(&A, A, dp);
}
static void fs94_ADC() {
    uint8_t dp = direct_indexed_X_8();
    ADC(&A, A, dp);
}
static void fs85_ADC() {
    uint8_t dp = absolute_8();
    ADC(&A, A, dp);
}
static void fs95_ADC() {
    uint8_t dp = absolute_indexed_X_8();
    ADC(&A, A, dp);
}
static void fs96_ADC() {
    uint8_t dp = absolute_indexed_Y_8();
    ADC(&A, A, dp);
}
static void fs89_ADC() {
    uint8_t dp = direct_8();
    program_counter += 1;
    uint8_t dp2 = direct_8();
    ADC(&dp, dp, dp2);
    program_counter += 2;
}
static void fs98_ADC() {
    uint8_t dp = direct_8();
    program_counter += 1;
    uint8_t dp2 = immediate_8();
    ADC(&dp, dp, dp2);
    program_counter += 2;
}
static void fs7A_ADDW() {
    uint16_t ya = YA();

}

static void fsB9_SBC() {
    SBC(&X, X, Y);
    program_counter += 1;
}
static void fsA8_SBC() {
    SBC(&A, A, immediate_8());
    program_counter += 2;
}
static void fsA6_SBC() {
    SBC(&A, A, X);
    program_counter += 1;
}
static void fsB7_SBC() {
    SBC(&A, A, direct_indirect_indexed_Y_indirect_8());
    program_counter += 2;
}
static void fsA7_SBC() {
    SBC(&A, A, direct_indexed_X_indirect_8());
    program_counter += 2;
}
static void fsA4_SBC() {
    SBC(&X, X, direct_8());
    program_counter += 2;
}
static void fsB4_SBC() {
    SBC(&A, A, direct_indexed_X_8());
    program_counter += 2;
}
static void fsA5_SBC() {
    SBC(&A, A, absolute_8());
    program_counter += 3;
}
static void fsB5_SBC() {
    SBC(&A, A, absolute_indexed_X_8());
    program_counter += 3;
}
static void fsB6_SBC() {
    SBC(&A, A, absolute_indexed_Y_8());
    program_counter += 3;
}
static void fsA9_SBC() {
    uint8_t dp = direct_8();
    program_counter += 1;
    uint8_t dp2 = direct_8();
    SBC(&dp, dp, dp2);
    program_counter += 3;
}
static void fsB8_SBC() {
    uint8_t dp = direct_8();
    program_counter += 1;
    uint8_t dp2 = immediate_8();
    SBC(&dp, dp, dp2);
    program_counter += 3;
}
static void fs9A_SUBW() {
    uint16_t ya = YA();

}

#pragma endregion 

#pragma region SPC_AND
static void AND(uint8_t B) {

}

static void fs39_AND() {
}
static void fs28_AND() {
}
static void fs26_AND() {
}
static void fs37_AND() {
}
static void fs27_AND() {
}
static void fs24_AND() {
}
static void fs34_AND() {
}
static void fs25_AND() {
}
static void fs35_AND() {
}
static void fs36_AND() {
}
static void fs29_AND() {
}
static void fs38_AND() {
}
static void fs6A_AND1() {
}
static void fs4A_AND1() {
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
static void BBC(uint8_t B) {

}

static void BBS(uint8_t B) {
}
static void fs13_BBC() {
}
static void fs33_BBC() {
}
static void fs53_BBC() {
}
static void fs73_BBC() {
}
static void fs93_BBC() {
}
static void fsB3_BBC() {
}
static void fsD3_BBC() {
}
static void fsF3_BBC() {
}
static void fs03_BBS() {
}
static void fs23_BBS() {
}
static void fs43_BBS() {
}
static void fs63_BBS() {
}
static void fs83_BBS() {
}
static void fsA3_BBS() {
}
static void fsC3_BBS() {
}
static void fsE3_BBS() {
}

static void fs90_BCC() {
}
static void fsB0_BCS() {
}
static void fsF0_BEQ() {
}
static void fs30_BMI() {
}
static void fsD0_BNE() {
}
static void fs10_BPL() {
}
static void fs50_BVC() {
}
static void fs70_BVS() {
}
static void fs2F_BRA() {
}
static void fs0F_BRK() {
}
static void fs1F_JMP() {
}
static void fs5F_JMP() {
}

#pragma endregion 

#pragma region SPC_CLR
static void CLR1(uint8_t B) {

}
static void fs12_CLR1() {
}
static void fs32_CLR1() {
}
static void fs52_CLR1() {
}
static void fs72_CLR1() {
}
static void fs92_CLR1() {
}
static void fsB2_CLR1() {
}
static void fsD2_CLR1() {
}
static void fsF2_CLR1() {
}

static void fs12_CLRC() {
}
static void fs12_CLRP() {
}
static void fs12_CLRB() {
}
#pragma endregion 

#pragma region SPC_CMP
static void CMP(uint8_t B) {

}
static void fs79_CMP() {
}
static void fs68_CMP() {
}
static void fs66_CMP() {
}
static void fs77_CMP() {
}
static void fs67_CMP() {
}
static void fs64_CMP() {
}
static void fs74_CMP() {
}
static void fs65_CMP() {
}
static void fs75_CMP() {
}
static void fs76_CMP() {
}
static void fsC8_CMP() {
}
static void fs3E_CMP() {
}
static void fs1E_CMP() {
}
static void fsAD_CMP() {
}
static void fs7E_CMP() {
}
static void fs5E_CMP() {
}
static void fs69_CMP() {
}
static void fs78_CMP() {
}
static void fs5A_CMPW() {
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
static void EOR(uint8_t B) {

}
static void OR(uint8_t B) {

}
static void fs59_EOR() {
}
static void fs48_EOR() {
}
static void fs46_EOR() {
}
static void fs57_EOR() {
}
static void fs47_EOR() {
}
static void fs44_EOR() {
}
static void fs54_EOR() {
}
static void fs45_EOR() {
}
static void fs55_EOR() {
}
static void fs56_EOR() {
}
static void fs49_EOR() {
}
static void fs58_EOR() {
}
static void fs8A_EOR1() {
}

static void fs19_OR() {
}
static void fs08_OR() {
}
static void fs06_OR() {
}
static void fs17_OR() {
}
static void fs07_OR() {
}
static void fs04_OR() {
}
static void fs14_OR() {
}
static void fs05_OR() {
}
static void fs15_OR() {
}
static void fs16_OR() {
}
static void fs09_OR() {
}
static void fs18_OR() {
}
static void fs2A_OR1() {
}
static void fs0A_OR1() {
}


#pragma endregion 

#pragma region SPC_MOV
static void MOV(uint8_t B) {

}
static void fsAF_MOV() {
}
static void fsC6_MOV() {
}
static void fsD7_MOV() {
}
static void fsC7_MOV() {
}
static void fsE8_MOV() {
}
static void fsE6_MOV() {
}
static void fsBF_MOV() {
}
static void fsF7_MOV() {
}
static void fsE7_MOV() {
}
static void fs7D_MOV() {
}
static void fsDD_MOV() {
}
static void fsE4_MOV() {
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
static void POP(uint8_t B) {

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
static void SET(uint8_t *A, uint8_t flags) {
    *A |= flags;
}
static void fs02_SET1() {
    SET(direct(0), 0x01);
    program_counter += 2;
}
static void fs22_SET1() {
    SET(direct(0), 0x02);
    program_counter += 2;
}
static void fs43_SET1() {
    SET(direct(0), 0x04);
    program_counter += 2;
}
static void fs62_SET1() {
    SET(direct(0), 0x08);
    program_counter += 2;
}
static void fs82_SET1() {
    SET(direct(0), 0x10);
    program_counter += 2;
}
static void fsA2_SET1() {
    SET(direct(0), 0x20);
    program_counter += 2;
}
static void fsC2_SET1() {
    SET(direct(0), 0x40);
    program_counter += 2;
}
static void fsE2_SET1() {
    SET(direct(0), 0x80);
    program_counter += 2;
}
static void fs80_SETC() {
    SET(&PSW, CARRY_FLAG);
    program_counter += 1;
}
static void fs40_SETP() {
    SET(&PSW, P_FLAG);
    program_counter += 1;
}
#pragma endregion

#pragma region SPC_TCALL
static void TCALL(uint16_t addr) {
    uint16_t pc_backup = program_counter;
    program_counter = addr;
    spc700_execute_next_instruction();
    program_counter = pc_backup;
}
static void fs01_TCALL() {
    TCALL(0xFFDE);
    program_counter += 1;
}
static void fs11_TCALL() {
    TCALL(0xFFDC);
    program_counter += 1;
}
static void fs21_TCALL() {
    TCALL(0xFFDA);
    program_counter += 1;
}
static void fs31_TCALL() {
    TCALL(0xFFD8);
    program_counter += 1;
}
static void fs41_TCALL() {
    TCALL(0xFFD6);
    program_counter += 1;
}
static void fs51_TCALL() {
    TCALL(0xFFD4);
    program_counter += 1;
}
static void fs61_TCALL() {
    TCALL(0xFFD2);
    program_counter += 1;
}
static void fs71_TCALL() {
    TCALL(0xFFD0);
    program_counter += 1;
}
static void fs81_TCALL() {
    TCALL(0xFFCE);
    program_counter += 1;
}
static void fs91_TCALL() {
    TCALL(0xFFCC);
    program_counter += 1;
}
static void fsA1_TCALL() {
    TCALL(0xFFCA);
    program_counter += 1;
}
static void fsB1_TCALL() {
    TCALL(0xFFC8);
    program_counter += 1;
}
static void fsC1_TCALL() {
    TCALL(0xFFC6);
    program_counter += 1;
}
static void fsD1_TCALL() {
    TCALL(0xFFC4);
    program_counter += 1;
}
static void fsE1_TCALL() {
    TCALL(0xFFC2);
    program_counter += 1;
}
static void fsF1_TCALL() {
    TCALL(0xFFC0);
    program_counter += 1;
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
static void fsC0_DI() {
}
static void fs9E_DIV() {
}
static void fsA0_EI() {
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
    uint8_t *a = absolute();
    uint16_t a_8 = *a;
    a_8 = a_8 & (~A);
    *a = a_8;
    uint8_t res = A - a_8;
    spc_set_PSW_register(res, ZERO_FLAG | NEGATIVE_FLAG);
    program_counter += 3;
}
static void fs0E_TSET1() {
    uint8_t *a = absolute();
    uint16_t a_8 = *a;
    a_8 = (a_8 | A);
    *a = a_8;
    uint8_t res = A - a_8;
    spc_set_PSW_register(res, ZERO_FLAG | NEGATIVE_FLAG);
    program_counter += 3;
}
static void fs9F_XCN() {
    A = (A >> 4) | (A << 4);
    program_counter += 1;
}

#pragma endregion

#pragma endregion