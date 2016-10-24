
#include "cpu.h"
#include "system.h"

_Bool inEmulationMode = 1, carryIsEmulationBit = 0;

uint32_t call_stack[30];
uint8_t cs_counter = 0;
_Bool accumulator_is_8bit = 1;
#pragma region private_functions

#pragma region addressing_modes
uint8_t immediate_8();
uint16_t immediate_16();

char relative();
short relative_long();

uint16_t direct();
uint32_t direct_indexed_x();
uint32_t direct_indexed_y();
uint32_t direct_indirect();
uint32_t direct_indexed_indirect();
uint32_t direct_indirect_indexed();
uint32_t direct_indirect_long();
uint32_t direct_indirect_indexed_long();

uint32_t absolute();
uint16_t absolute_indexed_x();
uint16_t absolute_indexed_y();
uint32_t absolute_long();
uint32_t absolute_indexed_long();
uint16_t absolute_indirect();
uint32_t absolute_indirect_long();
uint16_t absolute_indexed_indirect();

uint16_t stack_relative();
uint32_t stack_relative_indirect_indexed();



#pragma endregion

#pragma endregion


void initialise_cpu() {
	stack_pointer = access_address(0x7E0000);
	direct_page = (uint16_t)0000;
	populate_instructions();

}

void execute_next_instruction() {
	uint8_t *romLoc = getRomData();
	uint8_t current_instruction = (uint8_t) *(access_address(program_counter));
	(*instructions[current_instruction])();

}


#pragma region addressing_modes

uint8_t immediate_8() {
	uint8_t operand = (uint8_t) *(access_address(program_counter + 1));
	return operand;
}

uint16_t immediate_16() {
	uint8_t *addr = access_address(program_counter + 1);
	uint16_t operand = addr[1];
	operand = operand << 8;
	operand = operand | addr[0];
	return operand;
}

char relative() {
	uint8_t val = immediate_8();
	char s_val = (char)val;
	return s_val;
}

short relative_long() {
	uint16_t val = immediate_16();
	short s_val = (short)val;
	return s_val;
}

uint16_t direct() {
	uint8_t operand = immediate_8();
	uint16_t new_addr = direct_page + operand;
	return new_addr;
}

uint32_t direct_indexed_x() {
	uint8_t operand = immediate_8();
	return direct_page + operand + X;
}
uint32_t direct_indexed_y() {
	uint8_t operand = immediate_8();
	return direct_page + operand + Y;
}
uint32_t direct_indirect() {
	uint32_t new_addr_addr = direct();
	uint16_t new_addr = get2Byte(accessSystemRam()[new_addr_addr]);
	return data_bank_register + new_addr;
}
uint32_t direct_indexed_indirect() {
	uint8_t operand = immediate_8();
	uint16_t dp_addr = operand + direct_page + X;
	uint16_t new_addr = get2Byte(accessSystemRam()[dp_addr]);
	return (uint32_t)(data_bank_register + new_addr);
}
uint32_t direct_indirect_indexed() {
	uint8_t operand = immediate_8();
	uint16_t base_addr = get2Byte(accessSystemRam()[direct_page + operand]);
	uint32_t new_addr = data_bank_register + base_addr + Y;
	return new_addr;
}
uint32_t direct_indirect_long() {
	uint32_t new_addr_addr = direct();
	uint32_t new_addr = get3Byte(accessSystemRam()[new_addr_addr]);
	return new_addr;
}
uint32_t direct_indirect_indexed_long() {
	uint8_t operand = immediate_8();
	uint32_t new_addr = get3Byte(accessSystemRam()[direct_page + operand]);
	return new_addr;
}

uint32_t absolute() {
	uint16_t addr = immediate_16();
	return data_bank_register + addr;
}

uint16_t absolute_indexed_x() {
	return direct_page + immediate_16() + X;
}

uint16_t absolute_indexed_y() {
	return direct_page + immediate_16() + Y;
}

uint32_t absolute_long() {
	uint8_t *addr = access_address(program_counter + 1);
	uint32_t operand = addr[2];
	operand = operand << 8;
	operand = operand | addr[1];
	operand = operand << 8;
	operand = operand | addr[0];
	return operand;
}

uint32_t absolute_indexed_long() {
	return absolute_long() + X;
}

uint16_t absolute_indirect() {
	uint16_t operand = immediate_16();
	uint16_t addr = get2Byte(accessSystemRam()[operand]);
	return addr;
}
uint32_t absolute_indirect_long() {
	uint16_t operand = immediate_16();
	uint32_t addr = get3Byte(accessSystemRam()[operand]);
	return addr;
}
uint16_t absolute_indexed_indirect() {
	uint16_t operand = immediate_16();
	uint16_t addr = get2Byte(accessSystemRam()[operand + X]);
	return addr;
}

uint16_t stack_relative() {
	return stack_pointer + immediate_8();
}

uint32_t stack_relative_indirect_indexed() {
	uint32_t new_addr = data_bank_register + stack_pointer + immediate_8() + Y;
	return new_addr;
}

#pragma endregion


#pragma region INSTRUCTIONS

void populate_instructions() {
	instructions[0x00] = f00_BRK;
	instructions[0x01] = f01_ORA;
	instructions[0x02] = f02_COP;
	instructions[0x03] = f03_ORA;
	instructions[0x04] = f04_TSB;
	instructions[0x05] = f05_ORA;
	instructions[0x06] = f06_ASL;
	instructions[0x07] = f07_ORA;
	instructions[0x08] = f08_PHP;
	instructions[0x09] = f09_ORA;
	instructions[0x0A] = f0A_ASL;
	instructions[0x0B] = f0B_PHD;
	instructions[0x0C] = f0C_TSB;
	instructions[0x0D] = f0D_ORA;
	instructions[0x0E] = f0E_ASL;
	instructions[0x0F] = f0F_ORA;
	//
	instructions[0x10] = f10_BPL;
	instructions[0x11] = f11_ORA;
	instructions[0x10] = f10_BPL;
	instructions[0x11] = f11_ORA;
	instructions[0x12] = f12_ORA;
	instructions[0x13] = f13_ORA;
	instructions[0x14] = f14_TRB;
	instructions[0x15] = f15_ORA;
	instructions[0x16] = f16_ASL;
	instructions[0x17] = f17_ORA;
	instructions[0x18] = f18_CLC;
	instructions[0x19] = f19_ORA;
	instructions[0x1A] = f1A_INC;
	instructions[0x1B] = f1B_TCS;
	instructions[0x1C] = f1C_TRB;
	instructions[0x1D] = f1D_ORA;
	instructions[0x1E] = f1E_ASL;
	instructions[0x1F] = f1F_ORA;
	//
	instructions[0x20] = f20_JSR;
	instructions[0x21] = f21_AND;
	instructions[0x22] = f22_JSR;
	instructions[0x23] = f23_AND;
	instructions[0x24] = f24_BIT;
	instructions[0x25] = f25_AND;
	instructions[0x26] = f26_ROL;
	instructions[0x27] = f27_AND;
	instructions[0x28] = f28_PLP;
	instructions[0x29] = f29_AND;
	instructions[0x2A] = f2A_ROL;
	instructions[0x2B] = f2B_PLD;
	instructions[0x2C] = f2C_BIT;
	instructions[0x2D] = f2D_AND;
	instructions[0x2E] = f2E_ROL;
	instructions[0x2F] = f2F_AND;
	//
	instructions[0x30] = f30_BMI;
	instructions[0x31] = f31_AND;
	instructions[0x32] = f32_AND;
	instructions[0x33] = f33_AND;
	instructions[0x34] = f34_BIT;
	instructions[0x35] = f35_AND;
	instructions[0x36] = f36_ROL;
	instructions[0x37] = f37_AND;
	instructions[0x38] = f38_SEC;
	instructions[0x39] = f39_AND;
	instructions[0x3A] = f3A_DEC;
	instructions[0x3B] = f3B_TSC;
	instructions[0x3C] = f3C_BIT;
	instructions[0x3D] = f3D_AND;
	instructions[0x3E] = f3E_ROL;
	instructions[0x3F] = f3F_AND;
	//
	instructions[0x40] = f40_RTI;
	instructions[0x41] = f41_EOR;
	instructions[0x42] = f42_WDM;
	instructions[0x43] = f43_EOR;
	instructions[0x44] = f44_MVP;
	instructions[0x45] = f45_EOR;
	instructions[0x46] = f46_LSR;
	instructions[0x47] = f47_EOR;
	instructions[0x48] = f48_PHA;
	instructions[0x49] = f49_EOR;
	instructions[0x4A] = f4A_LSR;
	instructions[0x4B] = f4B_PHK;
	instructions[0x4C] = f4C_JMP;
	instructions[0x4D] = f4D_EOR;
	instructions[0x4E] = f4E_LSR;
	instructions[0x4F] = f4F_EOR;
	//
	instructions[0x50] = f50_BVC;
	instructions[0x51] = f51_EOR;
	instructions[0x52] = f52_EOR;
	instructions[0x53] = f53_EOR;
	instructions[0x54] = f54_MVN;
	instructions[0x55] = f55_EOR;
	instructions[0x56] = f56_LSR;
	instructions[0x57] = f57_EOR;
	instructions[0x58] = f58_CLI;
	instructions[0x59] = f59_EOR;
	instructions[0x5A] = f5A_PHY;
	instructions[0x5B] = f5B_TCD;
	instructions[0x5C] = f5C_JMP;
	instructions[0x5D] = f5D_EOR;
	instructions[0x5E] = f5E_LSR;
	instructions[0x5F] = f5F_EOR;
	//
	instructions[0x60] = f60_RTS;
	instructions[0x61] = f61_ADC;
	instructions[0x62] = f62_PER;
	instructions[0x63] = f63_ADC;
	instructions[0x64] = f64_STZ;
	instructions[0x65] = f65_ADC;
	instructions[0x66] = f66_ROR;
	instructions[0x67] = f67_ADC;
	instructions[0x68] = f68_PLA;
	instructions[0x69] = f69_ADC;
	instructions[0x6A] = f6A_ROR;
	instructions[0x6B] = f6B_RTL;
	instructions[0x6C] = f6C_JMP;
	instructions[0x6D] = f6D_ADC;
	instructions[0x6E] = f6E_ROR;
	instructions[0x6F] = f6F_ADC;
	//
	instructions[0x70] = f70_BVS;
	instructions[0x71] = f71_ADC;
	instructions[0x72] = f72_ADC;
	instructions[0x73] = f73_ADC;
	instructions[0x74] = f74_STZ;
	instructions[0x75] = f75_ADC;
	instructions[0x76] = f76_ROR;
	instructions[0x77] = f77_ADC;
	instructions[0x78] = f78_SEI;
	instructions[0x79] = f79_ADC;
	instructions[0x7A] = f7A_PLY;
	instructions[0x7B] = f7B_TDC;
	instructions[0x7C] = f7C_JMP;
	instructions[0x7D] = f7D_ADC;
	instructions[0x7E] = f7E_ROR;
	instructions[0x7F] = f7F_ADC;
	//
	instructions[0x80] = f80_BRA;
	instructions[0x81] = f81_STA;
	instructions[0x82] = f82_BRL;
	instructions[0x83] = f83_STA;
	instructions[0x84] = f84_STY;
	instructions[0x85] = f85_STA;
	instructions[0x86] = f86_STX;
	instructions[0x87] = f87_STA;
	instructions[0x88] = f88_DEY;
	instructions[0x89] = f89_BIT;
	instructions[0x8A] = f8A_TXA;
	instructions[0x8B] = f8B_PHB;
	instructions[0x8C] = f8C_STY;
	instructions[0x8D] = f8D_STA;
	instructions[0x8E] = f8E_STX;
	instructions[0x8F] = f8F_STA;
	//
	instructions[0x90] = f90_BCC;
	instructions[0x91] = f91_STA;
	instructions[0x92] = f92_STA;
	instructions[0x93] = f93_STA;
	instructions[0x94] = f94_STY;
	instructions[0x95] = f95_STA;
	instructions[0x96] = f96_STX;
	instructions[0x97] = f97_STA;
	instructions[0x98] = f98_TYA;
	instructions[0x99] = f99_STA;
	instructions[0x9A] = f9A_TXS;
	instructions[0x9B] = f9B_TXY;
	instructions[0x9C] = f9C_STZ;
	instructions[0x9D] = f9D_STA;
	instructions[0x9E] = f9E_STZ;
	instructions[0x9F] = f9F_STA;
	//
	instructions[0xA0] = fA0_LDY;
	instructions[0xA1] = fA1_LDA;
	instructions[0xA2] = fA2_LDX;
	instructions[0xA3] = fA3_LDA;
	instructions[0xA4] = fA4_LDY;
	instructions[0xA5] = fA5_LDA;
	instructions[0xA6] = fA6_LDX;
	instructions[0xA7] = fA7_LDA;
	instructions[0xA8] = fA8_TAY;
	instructions[0xA9] = fA9_LDA;
	instructions[0xAA] = fAA_TAX;
	instructions[0xAB] = fAB_PLB;
	instructions[0xAC] = fAC_LDY;
	instructions[0xAD] = fAD_LDA;
	instructions[0xAE] = fAE_LDX;
	instructions[0xAF] = fAF_LDA;
	//
	instructions[0xB0] = fB0_BCs;
	instructions[0xB1] = fB1_LDA;
	instructions[0xB2] = fB2_LDA;
	instructions[0xB3] = fB3_LDA;
	instructions[0xB4] = fB4_LDY;
	instructions[0xB5] = fB5_LDA;
	instructions[0xB6] = fB6_LDX;
	instructions[0xB7] = fB7_LDA;
	instructions[0xB8] = fB8_CLV;
	instructions[0xB9] = fB9_LDA;
	instructions[0xBA] = fBA_TSX;
	instructions[0xBB] = fBB_TYX;
	instructions[0xBC] = fBC_LDY;
	instructions[0xBD] = fBD_LDA;
	instructions[0xBE] = fBE_LDX;
	instructions[0xBF] = fBF_LDA;
	//
	instructions[0xC0] = fC0_CPY;
	instructions[0xC1] = fC1_CMP;
	instructions[0xC2] = fC2_REP;
	instructions[0xC3] = fC3_CMP;
	instructions[0xC4] = fC4_CPY;
	instructions[0xC5] = fC5_CMP;
	instructions[0xC6] = fC6_DEC;
	instructions[0xC7] = fC7_CMP;
	instructions[0xC8] = fC8_INY;
	instructions[0xC9] = fC9_CMP;
	instructions[0xCA] = fCA_DEX;
	instructions[0xCB] = fCB_WAI;
	instructions[0xCC] = fCC_CPY;
	instructions[0xCD] = fCD_CMP;
	instructions[0xCE] = fCE_DEC;
	instructions[0xCF] = fCF_CMP;
	//
	instructions[0xD0] = fD0_BNE;
	instructions[0xD1] = fD1_CMP;
	instructions[0xD2] = fD2_CMP;
	instructions[0xD3] = fD3_CMP;
	instructions[0xD4] = fD4_PEI;
	instructions[0xD5] = fD5_CMP;
	instructions[0xD6] = fD6_DEC;
	instructions[0xD7] = fD7_CMP;
	instructions[0xD8] = fD8_CLD;
	instructions[0xD9] = fD9_CMP;
	instructions[0xDA] = fDA_PHX;
	instructions[0xDB] = fDB_STP;
	instructions[0xDC] = fDC_JMP;
	instructions[0xDD] = fDD_CMP;
	instructions[0xDE] = fDE_DEC;
	instructions[0xDF] = fDF_CMP;
	//
	instructions[0xE0] = fE0_CPX;
	instructions[0xE1] = fE1_SBC;
	instructions[0xE2] = fE2_SEP;
	instructions[0xE3] = fE3_SBC;
	instructions[0xE4] = fE4_CPX;
	instructions[0xE5] = fE5_SBC;
	instructions[0xE6] = fE6_INC;
	instructions[0xE7] = fE7_SBC;
	instructions[0xE8] = fE8_INX;
	instructions[0xE9] = fE9_SBC;
	instructions[0xEA] = fEA_NOP;
	instructions[0xEB] = fEB_XBA;
	instructions[0xEC] = fEC_CPX;
	instructions[0xED] = fED_SBC;
	instructions[0xEE] = fEE_INC;
	instructions[0xEF] = fEF_SBC;
	//
	instructions[0xF0] = fF0_BEQ;
	instructions[0xF1] = fF1_SBC;
	instructions[0xF2] = fF2_SBC;
	instructions[0xF3] = fF3_SBC;
	instructions[0xF4] = fF4_PEA;
	instructions[0xF5] = fF5_SBC;
	instructions[0xF6] = fF6_INC;
	instructions[0xF7] = fF7_SBC;
	instructions[0xF8] = fF8_SED;
	instructions[0xF9] = fF9_SBC;
	instructions[0xFA] = fFA_PLX;
	instructions[0xFB] = fFB_XCE;
	instructions[0xFC] = fFC_JSR;
	instructions[0xFD] = fFD_SBC;
	instructions[0xFE] = fFE_INC;
	instructions[0xFF] = fFF_SBC;
	//

}

void ADC_8(uint8_t toAdd) {
	accumulator = accumulator + toAdd + (1 & p_register);
}
void ADC_16(uint16_t toAdd) {
	accumulator = accumulator + toAdd + (1 & p_register);
}
//ADC(_dp_, X)	61	DP Indexed Indirect, X	NV— - ZC	2
void f61_ADC(){
	uint32_t operand = direct_indexed_indirect();
	uint8_t data = access_address(operand);
	ADC_8(data);
	program_counter += 2;
}

//ADC sr, S	63	Stack Relative	NV— - ZC	2
void f63_ADC(){
	uint16_t stack_addr = stack_relative();
	uint8_t data = accessSystemRam()[stack_addr];
	ADC_8(data);
	program_counter += 2;
}

//ADC dp	65	Direct Page	NV— - ZC	2
void f65_ADC(){
	uint16_t new_page = direct();
	uint8_t data = accessSystemRam()[new_page];
	ADC_8(data);
	program_counter += 2;
}

//ADC[_dp_]	67	DP Indirect Long	NV— - ZC	2
void f67_ADC(){
	uint32_t new_addr = direct_indirect_long();
	uint8_t data = (uint8_t) *access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC #const	69	Immediate	NV— - ZC	23
void f69_ADC(){
	ADC_8(immediate_8);
	program_counter += 2;
}

//ADC addr	6D	Absolute	NV— - ZC	3
void f6D_ADC(){
	uint32_t new_addr = absolute();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 3;
}

//ADC long	6F	Absolute Long	NV— - ZC	4
void f6F_ADC(){
	uint32_t new_addr = absolute_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 4;
}

//ADC(dp), Y	71	DP Indirect Indexed, Y	NV— - ZC	2
void f71_ADC(){
	uint32_t new_addr = direct_indirect_indexed();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC(_dp_)	72	DP Indirect	NV— - ZC	2
void f72_ADC(){
	uint32_t new_addr = direct_indirect();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC(_sr_, S), Y	73	SR Indirect Indexed, Y	NV— - ZC	2
void f73_ADC(){
	uint32_t new_addr = stack_relative_indirect_indexed();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC dp, X	75	DP Indexed, X	NV— - ZC	2
void f75_ADC(){
	uint32_t new_addr = direct_indexed_x();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC[_dp_], Y	77	DP Indirect Long Indexed, Y	NV— - ZC	2
void f77_ADC(){
	uint32_t new_addr = direct_indirect_indexed_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 2;
}

//ADC addr, Y	79	Absolute Indexed, Y	NV— - ZC	3
void f79_ADC(){
	uint32_t new_addr = absolute_indexed_y();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 3;
}

//ADC addr, X	7D	Absolute Indexed, X	NV— - ZC	3
void f7D_ADC(){
	uint32_t new_addr = absolute_indexed_x();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 3;
}

//ADC long, X	7F	Absolute Long Indexed, X	NV— - ZC	4
void f7F_ADC(){
	uint32_t new_addr = absolute_indexed_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	ADC_8(data);
	program_counter += 4;
}

//AND(_dp, _X)	21	DP Indexed Indirect, X	N—–Z - 2
void f21_AND(){}

//AND sr, S	23	Stack Relative	N—–Z - 2
void f23_AND(){}

//AND dp	25	Direct Page	N—–Z - 2
void f25_AND(){}

//AND[_dp_]	27	DP Indirect Long	N—–Z - 2
void f27_AND(){}

//AND #const	29	Immediate	N—–Z - 2
void f29_AND(){}

//AND addr	2D	Absolute	N—–Z - 3
void f2D_AND(){}

//AND long	2F	Absolute Long	N—–Z - 4
void f2F_AND(){}

//AND(_dp_), Y	31	DP Indirect Indexed, Y	N—–Z - 2
void f31_AND(){}

//AND(_dp_)	32	DP Indirect	N—–Z - 2
void f32_AND(){}

//AND(_sr_, S), Y	33	SR Indirect Indexed, Y	N—–Z - 2
void f33_AND(){}

//AND dp, X	35	DP Indexed, X	N—–Z - 2
void f35_AND(){}

//AND[_dp_], Y	37	DP Indirect Long Indexed, Y	N—–Z - 2
void f37_AND(){}

//AND addr, Y	39	Absolute Indexed, Y	N—–Z - 3
void f39_AND(){}

//AND addr, X	3D	Absolute Indexed, X	N—–Z - 3
void f3D_AND(){}

//AND long, X	3F	Absolute Long Indexed, X	N—–Z - 4
void f3F_AND(){}

//ASL dp	6	Direct Page	N—–ZC	2
void f06_ASL(){}

//ASL A	0A	Accumulator	N—–ZC	1
void f0A_ASL(){}

//ASL addr	0E	Absolute	N—–ZC	3
void f0E_ASL(){}

//ASL dp, X	16	DP Indexed, X	N—–ZC	2
void f16_ASL(){}

//ASL addr, X	1E	Absolute Indexed, X	N—–ZC	3
void f1E_ASL(){}

//BCC nearlabel	90	Program Counter Relative		2
void f90_BCC(){}

//BCS nearlabel	B0	Program Counter Relative		2
void fB0_BCs(){}

//BEQ nearlabel	F0	Program Counter Relative		2
void fF0_BEQ(){}

//BIT dp	24	Direct Page	NV— - Z - 2
void f24_BIT(){}

//BIT addr	2C	Absolute	NV— - Z - 3
void f2C_BIT(){}

//BIT dp, X	34	DP Indexed, X	NV— - Z - 2
void f34_BIT(){}

//BIT addr, X	3C	Absolute Indexed, X	NV— - Z - 3
void f3C_BIT(){}

//BIT #const	89	Immediate	——Z - 2
void f89_BIT(){}

//BMI nearlabel	30	Program Counter Relative		2
void f30_BMI(){}

//BNE nearlabel	D0	Program Counter Relative		2
void fD0_BNE(){}

//BPL nearlabel	10	Program Counter Relative		2
void f10_BPL(){}

//BRA nearlabel	80	Program Counter Relative		2
void f80_BRA(){}

//BRK	0	Stack / Interrupt	— - DI–	28
void f00_BRK(){}

//BRL label	82	Program Counter Relative Long		3
void f82_BRL(){}

//BVC nearlabel	50	Program Counter Relative		2
void f50_BVC(){}

//BVS nearlabel	70	Program Counter Relative		2
void f70_BVS(){}

//CLC	18	Implied	—— - C	1
void f18_CLC(){}

//CLD	D8	Implied	— - D—	1
void fD8_CLD(){}

//CLI	58	Implied	—–I–	1
void f58_CLI(){}

//CLV	B8	Implied	#NAME ? 1
void fB8_CLV(){}

//CMP(_dp, _X)	C1	DP Indexed Indirect, X	N—–ZC	2
void fC1_CMP(){}

//CMP sr, S	C3	Stack Relative	N—–ZC	2
void fC3_CMP(){}

//CMP dp	C5	Direct Page	N—–ZC	2
void fC5_CMP(){}

//CMP[_dp_]	C7	DP Indirect Long	N—–ZC	2
void fC7_CMP(){}

//CMP #const	C9	Immediate	N—–ZC	2
void fC9_CMP(){}

//CMP addr	CD	Absolute	N—–ZC	3
void fCD_CMP(){}

//CMP long	CF	Absolute Long	N—–ZC	4
void fCF_CMP(){}

//CMP(_dp_), Y	D1	DP Indirect Indexed, Y	N—–ZC	2
void fD1_CMP(){}

//CMP(_dp_)	D2	DP Indirect	N—–ZC	2
void fD2_CMP(){}

//CMP(_sr_, S), Y	D3	SR Indirect Indexed, Y	N—–ZC	2
void fD3_CMP(){}

//CMP dp, X	D5	DP Indexed, X	N—–ZC	2
void fD5_CMP(){}

//CMP[_dp_], Y	D7	DP Indirect Long Indexed, Y	N—–ZC	2
void fD7_CMP(){}

//CMP addr, Y	D9	Absolute Indexed, Y	N—–ZC	3
void fD9_CMP(){}

//CMP addr, X	DD	Absolute Indexed, X	N—–ZC	3
void fDD_CMP(){}

//CMP long, X	DF	Absolute Long Indexed, X	N—–ZC	4
void fDF_CMP(){}

//COP const	2	Stack / Interrupt	— - DI–	2
void f02_COP(){}

//CPX #const	E0	Immediate	N—–ZC	210
void fE0_CPX(){}

//CPX dp	E4	Direct Page	N—–ZC	2
void fE4_CPX(){}

//CPX addr	EC	Absolute	N—–ZC	3
void fEC_CPX(){}

//CPY #const	C0	Immediate	N—–ZC	2
void fC0_CPY(){}

//CPY dp	C4	Direct Page	N—–ZC	2
void fC4_CPY(){}

//CPY addr	CC	Absolute	N—–ZC	3
void fCC_CPY(){}

//DEC A	3A	Accumulator	N—–Z - 1
void f3A_DEC(){}

//DEC dp	C6	Direct Page	N—–Z - 2
void fC6_DEC(){}

//DEC addr	CE	Absolute	N—–Z - 3
void fCE_DEC(){}

//DEC dp, X	D6	DP Indexed, X	N—–Z - 2
void fD6_DEC(){}

//DEC addr, X	DE	Absolute Indexed, X	N—–Z - 3
void fDE_DEC(){}

//DEX	CA	Implied	N—–Z - 1
void fCA_DEX(){}

//DEY	88	Implied	N—–Z - 1
void f88_DEY(){}

//EOR(_dp, _X)	41	DP Indexed Indirect, X	N—–Z - 2
void f41_EOR(){}

//EOR sr, S	43	Stack Relative	N—–Z - 2
void f43_EOR(){}

//EOR dp	45	Direct Page	N—–Z - 2
void f45_EOR(){}

//EOR[_dp_]	47	DP Indirect Long	N—–Z - 2
void f47_EOR(){}

//EOR #const	49	Immediate	N—–Z - 2
void f49_EOR(){}

//EOR addr	4D	Absolute	N—–Z - 3
void f4D_EOR(){}

//EOR long	4F	Absolute Long	N—–Z - 4
void f4F_EOR(){}

//EOR(_dp_), Y	51	DP Indirect Indexed, Y	N—–Z - 2
void f51_EOR(){}

//EOR(_dp_)	52	DP Indirect	N—–Z - 2
void f52_EOR(){}

//EOR(_sr_, S), Y	53	SR Indirect Indexed, Y	N—–Z - 2
void f53_EOR(){}

//EOR dp, X	55	DP Indexed, X	N—–Z - 2
void f55_EOR(){}

//EOR[_dp_], Y	57	DP Indirect Long Indexed, Y	N—–Z - 2
void f57_EOR(){}

//EOR addr, Y	59	Absolute Indexed, Y	N—–Z - 3
void f59_EOR(){}

//EOR addr, X	5D	Absolute Indexed, X	N—–Z - 3
void f5D_EOR(){}

//EOR long, X	5F	Absolute Long Indexed, X	N—–Z - 4
void f5F_EOR(){}

//INC A	1A	Accumulator	N—–Z - 1
void f1A_INC(){}

//INC dp	E6	Direct Page	N—–Z - 2
void fE6_INC(){}

//INC addr	EE	Absolute	N—–Z - 3
void fEE_INC(){}

//INC dp, X	F6	DP Indexed, X	N—–Z - 2
void fF6_INC(){}

//INC addr, X	FE	Absolute Indexed, X	N—–Z - 3
void fFE_INC(){}

//INX	E8	Implied	N—–Z - 1
void fE8_INX(){}

//INY	C8	Implied	N—–Z - 1
void fC8_INY(){}

//JMP addr	4C	Absolute		3
void f4C_JMP(){}

//JMP long	5C	Absolute Long		4
void f5C_JMP(){
	uint8_t bank = (uint8_t) *(access_address(program_counter + 3));
	uint16_t addr = get2Byte(access_address( program_counter + 1));
	program_counter = gen3Byte(bank, addr);
}

//JMP(_addr_)	6C	Absolute Indirect		3
void f6C_JMP(){}

//JMP(_addr, X_)	7C	Absolute Indexed Indirect		3
void f7C_JMP(){}

//JMP[addr]	DC	Absolute Indirect Long		3
void fDC_JMP(){}

//JSR addr	20	Absolute		3
void f20_JSR(){
	uint16_t operand = absolute();
	uint32_t addr = program_counter ;
	addr = addr & 0xFFFF0000;
	addr = addr | operand;
	call_stack[cs_counter++] = program_counter;
	program_counter = addr;

}

//JSR long	22	Absolute Long		4
void f22_JSR(){
	uint32_t operand = absolute_long(program_counter + 1);
	call_stack[cs_counter++] = program_counter;
	program_counter = operand;

}

//JSR(addr, X))	FC	Absolute Indexed Indirect		3
void fFC_JSR(){
}

//LDA(_dp, _X)	A1	DP Indexed Indirect, X	N—–Z - 2
void fA1_LDA(){}

//LDA sr, S	A3	Stack Relative	N—–Z - 2
void fA3_LDA(){}

//LDA dp	A5	Direct Page	N—–Z - 2
void fA5_LDA(){}

//LDA[_dp_]	A7	DP Indirect Long	N—–Z - 2
void fA7_LDA(){}

//LDA #const	A9	Immediate	N—–Z - 2
void fA9_LDA(){
	if (p_register & 0x20) {
		uint8_t operand = (uint8_t)*(access_address(program_counter + 1));
		accumulator &= 0xFF00;
		accumulator |= (0x00FF & operand);
		program_counter += 2;
	}
	else {
		uint16_t operand = immediate_16();
		accumulator = operand;
		program_counter += 3;
	}
	return;
	
}

//LDA addr	AD	Absolute	N—–Z - 3
void fAD_LDA(){}

//LDA long	AF	Absolute Long	N—–Z - 4
void fAF_LDA(){}

//LDA(_dp_), Y	B1	DP Indirect Indexed, Y	N—–Z - 2
void fB1_LDA(){}

//LDA(_dp_)	B2	DP Indirect	N—–Z - 2
void fB2_LDA(){}

//LDA(_sr_, S), Y	B3	SR Indirect Indexed, Y	N—–Z - 2
void fB3_LDA(){}

//LDA dp, X	B5	DP Indexed, X	N—–Z - 2
void fB5_LDA(){}

//LDA[_dp_], Y	B7	DP Indirect Long Indexed, Y	N—–Z - 2
void fB7_LDA(){}

//LDA addr, Y	B9	Absolute Indexed, Y	N—–Z - 3
void fB9_LDA(){}

//LDA addr, X	BD	Absolute Indexed, X	N—–Z - 3
void fBD_LDA(){}

//LDA long, X	BF	Absolute Long Indexed, X	N—–Z - 4
void fBF_LDA(){}

//LDX #const	A2	Immediate	N—–Z - 212
void fA2_LDX(){}

//LDX dp	A6	Direct Page	N—–Z - 2
void fA6_LDX(){}

//LDX addr	AE	Absolute	N—–Z - 3
void fAE_LDX(){}

//LDX dp, Y	B6	DP Indexed, Y	N—–Z - 2
void fB6_LDX(){}

//LDX addr, Y	BE	Absolute Indexed, Y	N—–Z - 3
void fBE_LDX(){}

//LDY #const	A0	Immediate	N—–Z - 2
void fA0_LDY(){}

//LDY dp	A4	Direct Page	N—–Z - 2
void fA4_LDY(){}

//LDY addr	AC	Absolute	N—–Z - 3
void fAC_LDY(){}

//LDY dp, X	B4	DP Indexed, X	N—–Z - 2
void fB4_LDY(){}

//LDY addr, X	BC	Absolute Indexed, X	N—–Z - 3
void fBC_LDY(){}

//LSR dp	46	Direct Page	N—–ZC	2
void f46_LSR(){}

//LSR A	4A	Accumulator	N—–ZC	1
void f4A_LSR(){}

//LSR addr	4E	Absolute	N—–ZC	3
void f4E_LSR(){}

//LSR dp, X	56	DP Indexed, X	N—–ZC	2
void f56_LSR(){}

//LSR addr, X	5E	Absolute Indexed, X	N—–ZC	3
void f5E_LSR(){}

//MVN srcbk, destbk	54	Block Move		3
void f54_MVN(){}

//MVP srcbk, destbk	44	Block Move		3
void f44_MVP(){}

//NOP	EA	Implied		1
void fEA_NOP(){}

//ORA(_dp, _X)	1	DP Indexed Indirect, X	N—–Z - 2
void f01_ORA(){
	short *operand = access_address(program_counter + 1);
	short *dpVal = access_address(direct_page);
	short *value = access_address(*dpVal + *operand);
	accumulator = accumulator | *value;
	program_counter += 2;
}

//ORA sr, S	3	Stack Relative	N—–Z - 2
void f03_ORA(){
	accumulator = accumulator | *((short*)access_address(stack_pointer));
}

//ORA dp	5	Direct Page	N—–Z - 2
void f05_ORA(){
	accumulator = accumulator | *((short*)access_address(direct_page));
}

//ORA[_dp_]	7	DP Indirect Long	N—–Z - 2
void f07_ORA(){
	short *operand = access_address(program_counter + 1);
	short *dpVal = access_address(direct_page);
	int *value = access_address(*dpVal);
	accumulator = accumulator | *value;
	program_counter += 2;
}

//ORA #const	9	Immediate	N—–Z - 2
void f09_ORA(){}

//ORA addr	0D	Absolute	N—–Z - 3
void f0D_ORA(){}

//ORA long	0F	Absolute Long	N—–Z - 4
void f0F_ORA(){}

//ORA(_dp_), Y	11	DP Indirect Indexed, Y	N—–Z - 2
void f11_ORA(){}

//ORA(_dp_)	12	DP Indirect	N—–Z - 2
void f12_ORA(){}

//ORA(_sr_, S), Y	13	SR Indirect Indexed, Y	N—–Z - 2
void f13_ORA(){}

//ORA dp, X	15	DP Indexed, X	N—–Z - 2
void f15_ORA(){}

//ORA[_dp_], Y	17	DP Indirect Long Indexed, Y	N—–Z - 2
void f17_ORA(){}

//ORA addr, Y	19	Absolute Indexed, Y	N—–Z - 3
void f19_ORA(){}

//ORA addr, X	1D	Absolute Indexed, X	N—–Z - 3
void f1D_ORA(){}

//ORA long, X	1F	Absolute Long Indexed, X	N—–Z - 4
void f1F_ORA(){}

//PEA addr	F4	Stack(Absolute)		3
void fF4_PEA(){}

//PEI(dp)	D4	Stack(DP Indirect)		2
void fD4_PEI(){}

//PER label	62	Stack(PC Relative Long)		3
void f62_PER(){}

//PHA	48	Stack(Push)		1
void f48_PHA(){}

//PHB	8B	Stack(Push)		1
void f8B_PHB(){
	store2Byte(stack_pointer, (uint16_t)(program_counter >> 16));
	stack_pointer -= 2;
	program_counter++;
}

//PHD	0B	Stack(Push)		1
void f0B_PHD(){}

//PHK	4B	Stack(Push)		1
void f4B_PHK(){
	store2Byte(stack_pointer, (uint16_t)(program_counter >> 16));
	stack_pointer -= 2;
	program_counter++;
}

//PHP	8	Stack(Push)		1
void f08_PHP(){}

//PHX	DA	Stack(Push)		1
void fDA_PHX(){}

//PHY	5A	Stack(Push)		1
void f5A_PHY(){}

//PLA	68	Stack(Pull)	N—–Z - 1
void f68_PLA(){}

//PLB	AB	Stack(Pull)	N—–Z - 1
void fAB_PLB(){
	stack_pointer += 2;
	data_bank_register = get2Byte(accessSystemRam() + (long) stack_pointer);
	program_counter += 1;
}

//PLD	2B	Stack(Pull)	N—–Z - 1
void f2B_PLD(){}

//PLP	28	Stack(Pull)	N—–Z - 1
void f28_PLP(){}

//PLX	FA	Stack(Pull)	N—–Z - 1
void fFA_PLX(){}

//PLY	7A	Stack(Pull)	N—–Z - 1
void f7A_PLY(){}

//REP #const	C2	Immediate	NVMXDIZC	2
void fC2_REP(){
	uint8_t operand = (uint8_t) *(access_address(program_counter + 1));
	p_register = p_register & !(operand);
	program_counter += 2;
}

//ROL dp	26	Direct Page	N—–ZC	2
void f26_ROL(){}

//ROL A	2A	Accumulator	N—–ZC	1
void f2A_ROL(){}

//ROL addr	2E	Absolute	N—–ZC	3
void f2E_ROL(){}

//ROL dp, X	36	DP Indexed, X	N—–ZC	2
void f36_ROL(){}

//ROL addr, X	3E	Absolute Indexed, X	N—–ZC	3
void f3E_ROL(){}

//ROR dp	66	Direct Page	N—–ZC	2
void f66_ROR(){}

//ROR A	6A	Accumulator	N—–ZC	1
void f6A_ROR(){}

//ROR addr	6E	Absolute	N—–ZC	3
void f6E_ROR(){}

//ROR dp, X	76	DP Indexed, X	N—–ZC	2
void f76_ROR(){}

//ROR addr, X	7E	Absolute Indexed, X	N—–ZC	3
void f7E_ROR(){}

//RTI	40	Stack(RTI)	NVMXDIZC	1
void f40_RTI(){}

//RTL	6B	Stack(RTL)		1
void f6B_RTL(){}

//RTS	60	Stack(RTS)		1
void f60_RTS(){}

//SBC(_dp, _X)	E1	DP Indexed Indirect, X	NV— - ZC	2
void fE1_SBC(){}

//SBC sr, S	E3	Stack Relative	NV— - ZC	2
void fE3_SBC(){}

//SBC dp	E5	Direct Page	NV— - ZC	2
void fE5_SBC(){}

//SBC[_dp_]	E7	DP Indirect Long	NV— - ZC	2
void fE7_SBC(){}

//SBC #const	E9	Immediate	NV— - ZC	2
void fE9_SBC(){}

//SBC addr	ED	Absolute	NV— - ZC	3
void fED_SBC(){}

//SBC long	EF	Absolute Long	NV— - ZC	4
void fEF_SBC(){}

//SBC(_dp_), Y	F1	DP Indirect Indexed, Y	NV— - ZC	2
void fF1_SBC(){}

//SBC(_dp_)	F2	DP Indirect	NV— - ZC	2
void fF2_SBC(){}

//SBC(_sr_, S), Y	F3	SR Indirect Indexed, Y	NV— - ZC	2
void fF3_SBC(){}

//SBC dp, X	F5	DP Indexed, X	NV— - ZC	2
void fF5_SBC(){}

//SBC[_dp_], Y	F7	DP Indirect Long Indexed, Y	NV— - ZC	2
void fF7_SBC(){}

//SBC addr, Y	F9	Absolute Indexed, Y	NV— - ZC	3
void fF9_SBC(){}

//SBC addr, X	FD	Absolute Indexed, X	NV— - ZC	3
void fFD_SBC(){}

//SBC long, X	FF	Absolute Long Indexed, X	NV— - ZC	4
void fFF_SBC(){}

//SEC	38	Implied	—— - C	1
void f38_SEC(){}

//SED	F8	Implied	— - D—	1
void fF8_SED(){}

//SEI	78	Implied	—–I–	1
void f78_SEI(){
	p_register = p_register & 0xFB;
	program_counter++;
}

//SEP	E2	Immediate	NVMXDIZC	2
void fE2_SEP(){
	uint8_t operand = (uint8_t) *(access_address(program_counter + 1));
	p_register |= operand;
	if (carryIsEmulationBit && (operand & 0x01))
		inEmulationMode = !inEmulationMode;
	if (!inEmulationMode && (operand & 0x20))
		accumulator_is_8bit = 1;
	program_counter += 2;
}

//STA(_dp, _X)	81	DP Indexed Indirect, X		2
void f81_STA(){}

//STA sr, S	83	Stack Relative		2
void f83_STA(){}

//STA dp	85	Direct Page		2
void f85_STA(){}

//STA[_dp_]	87	DP Indirect Long		2
void f87_STA(){}

//STA addr	8D	Absolute		3
void f8D_STA(){
	uint8_t *mem_mapped = access_address(program_counter + 1);
	uint16_t addr = get2Byte(mem_mapped);
	store2Byte(addr, accumulator);
	program_counter += 3;
}

//STA long	8F	Absolute Long		4
void f8F_STA(){
	uint32_t operand = absolute_long();
	uint8_t *addr = access_address(operand);
	addr[0] = (uint8_t)(accumulator & 0xFF00);
	addr[1] = (uint8_t)(accumulator & 0x00FF);
}

//STA(_dp_), Y	91	DP Indirect Indexed, Y		2
void f91_STA(){}

//STA(_dp_)	92	DP Indirect		2
void f92_STA(){}

//STA(_sr_, S), Y	93	SR Indirect Indexed, Y		2
void f93_STA(){}

//STA dpX	95	DP Indexed, X		2
void f95_STA(){}

//STA[_dp_], Y	97	DP Indirect Long Indexed, Y		2
void f97_STA(){}

//STA addr, Y	99	Absolute Indexed, Y		3
void f99_STA(){}

//STA addr, X	9D	Absolute Indexed, X		3
void f9D_STA(){}

//STA long, X	9F	Absolute Long Indexed, X		4
void f9F_STA(){}

//STP	DB	Implied		1
void fDB_STP(){}

//STX dp	86	Direct Page		2
void f86_STX(){}

//STX addr	8E	Absolute		3
void f8E_STX(){}

//STX dp, Y	96	DP Indexed, Y		2
void f96_STX(){}

//STY dp	84	Direct Page		2
void f84_STY(){}

//STY addr	8C	Absolute		3
void f8C_STY(){}

//STY dp, X	94	DP Indexed, X		2
void f94_STY(){}

//STZ dp	64	Direct Page		2
void f64_STZ(){}

//STZ dp, X	74	DP Indexed, X		2
void f74_STZ(){}

//STZ addr	9C	Absolute		3
void f9C_STZ(){
	uint8_t *addr = access_address(get2Byte(access_address(program_counter + 1)));
	store2Byte(addr, 0x0000);
	program_counter += 3;
}

//STZ addr, X	9E	Absolute Indexed, X		3
void f9E_STZ(){}

//TAX	AA	Implied	N—–Z - 1
void fAA_TAX(){}

//TAY	A8	Implied	N—–Z - 1
void fA8_TAY(){}

//TCD	5B	Implied	N—–Z - 1
void f5B_TCD(){}

//TCS	1B	Implied		1
void f1B_TCS(){
	stack_pointer = accumulator;
	program_counter++;
}

//TDC	7B	Implied	N—–Z - 1
void f7B_TDC(){}

//TRB dp	14	Direct Page	——Z - 2
void f14_TRB(){}

//TRB addr	1C	Absolute	——Z - 3
void f1C_TRB(){}

//TSB dp	4	Direct Page	——Z - 2
void f04_TSB(){}

//TSB addr	0C	Absolute	——Z - 3
void f0C_TSB(){}

//TSC	3B	Implied	N—–Z - 1
void f3B_TSC(){}

//TSX	BA	Implied	N—–Z - 1
void fBA_TSX(){}

//TXA	8A	Implied	N—–Z - 1
void f8A_TXA(){}

//TXS	9A	Implied		1
void f9A_TXS(){}

//TXY	9B	Implied	N—–Z - 1
void f9B_TXY(){}

//TYA	98	Implied	N—–Z - 1
void f98_TYA(){}

//TYX	BB	Implied	N—–Z - 1
void fBB_TYX(){}

//WAI	CB	Implied		1
void fCB_WAI(){}

//WDM	42			2
void f42_WDM(){}

//XBA	EB	Implied	N—–Z - 1
void fEB_XBA(){
	uint8_t B = accumulator & 0x00FF;
	accumulator = accumulator >> 8;
	accumulator = accumulator | (B << 8);
	program_counter++;
}

//XCE	FB	Implied	–MX—CE	1
void fFB_XCE(){
	inEmulationMode = !inEmulationMode;
	program_counter++;
}


#pragma endregion