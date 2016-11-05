
#include "cpu.h"
#include "system.h"
#include <string.h>
#include <stdio.h>

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

uint8_t direct_8();
uint16_t direct_16();
uint8_t direct_indexed_x_8();
uint16_t direct_indexed_x_16();

uint8_t direct_indexed_y_8();
uint16_t direct_indexed_y_16();

uint8_t direct_indirect_8();
uint16_t direct_indirect_16();
uint8_t direct_indexed_indirect_8();
uint16_t direct_indexed_indirect_16();

uint8_t direct_indirect_indexed_8();
uint16_t direct_indirect_indexed_16();

uint8_t direct_indirect_long_8();
uint16_t direct_indirect_long_16();

uint8_t direct_indirect_indexed_long_8();
uint16_t direct_indirect_indexed_long_16();

uint8_t absolute_8();
uint16_t absolute_16();
uint8_t absolute_indexed_x_8();
uint16_t absolute_indexed_x_16();
uint8_t absolute_indexed_y_8();
uint16_t absolute_indexed_y_16();

uint8_t absolute_long_8();
uint16_t absolute_long_16();

uint8_t absolute_indexed_long_8();
uint16_t absolute_indexed_long_16();

uint8_t absolute_indirect_8();
uint16_t absolute_indirect_16();

uint8_t absolute_indirect_long_8();
uint16_t absolute_indirect_long_16();

uint8_t absolute_indexed_indirect_8();
uint16_t absolute_indexed_indirect_16();

uint8_t stack_relative_8();
uint16_t stack_relative_16();

uint8_t stack_relative_indirect_indexed_8();
uint16_t stack_relative_indirect_indexed_16();

#pragma endregion

#pragma endregion


void initialise_cpu() {
	stack_pointer = access_address(0x7E0000);
	direct_page = (uint16_t)0000;
	p_register = 0x34;
	inEmulationMode = 1;
	emulation_flag = 0x1;
	populate_instructions();

}
const char *byte_to_binary(uint8_t x) {
	static char b[9];
	b[0] = '\0';

	int z;
	for (z = 128; z > 0; z >>= 1) {
		strcat(b, ((x & z) == z) ? "1" : "0");
	}

	return b;
}

void execute_next_instruction() {
	static int counter = 0;
	counter++;
	uint8_t *romLoc = getRomData();
	uint8_t current_instruction = (uint8_t) *(access_address(program_counter));
	printf("%03i | %06x | %02x | A:%04x | X:%04x | Y:%04x | P:%s\n", counter, program_counter, current_instruction, accumulator, X, Y, byte_to_binary(p_register));
	(*instructions[current_instruction])();
}

void set_p_register_16(uint16_t val, uint8_t flags) {
	if (val == 0)
		p_register |= ZERO_FLAG & flags;
	if (val & 0x80000000 == 1)
		p_register |= NEGATIVE_FLAG & flags;

}
void set_p_register_8(uint8_t val, uint8_t flags) {
	if (val == 0)
		p_register |= ZERO_FLAG & flags;
	if (val & 0x8000 == 1)
		p_register |= NEGATIVE_FLAG & flags;
}
int m_flag() {
	uint8_t m_val = p_register & M_FLAG;
	if (m_val > 0)
		return 1;
	else
		return 0;
}

int x_flag() {
	uint8_t x_val = p_register & X_FLAG;
	if (x_val > 0)
		return 1;
	else
		return 0;
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
	uint32_t system_addr = data_bank_register;
	system_addr <<= 16;
	system_addr |= addr;
	return system_addr;
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

#pragma region data_access

uint8_t direct_8() {
	uint16_t new_page = direct();
	uint8_t data = accessSystemRam()[new_page];
};
uint16_t direct_16() {
	uint16_t new_page = direct();
	uint16_t data = get2Byte(accessSystemRam()[new_page]);
};

uint8_t direct_indexed_x_8() {
	uint32_t new_addr = direct_indexed_x();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
};
uint16_t direct_indexed_x_16() {
	uint32_t new_addr = direct_indexed_x();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
};

uint8_t direct_indexed_y_8() {
	uint32_t new_addr = direct_indexed_y();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
};
uint16_t direct_indexed_y_16() {
	uint32_t new_addr = direct_indexed_y();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
};

uint8_t direct_indirect_8() {
	uint32_t new_addr = direct_indirect();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
};
uint16_t direct_indirect_16() {
	uint32_t new_addr = direct_indirect();
	uint8_t data = get2Byte(access_address(new_addr));
	return data;
};

uint8_t direct_indexed_indirect_8() {
	uint32_t operand = direct_indexed_indirect();
	uint8_t data = access_address(operand)[0];
	return data;
}
uint16_t direct_indexed_indirect_16() {
	uint32_t operand = direct_indexed_indirect();
	uint16_t data = get2Byte(access_address(operand));
	return data;
}


uint8_t direct_indirect_indexed_8() {
	uint32_t new_addr = direct_indirect_indexed();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t direct_indirect_indexed_16() {
	uint32_t new_addr = direct_indirect_indexed();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t direct_indirect_long_8() {
	uint32_t new_addr = direct_indirect_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t direct_indirect_long_16() {
	uint32_t new_addr = direct_indirect_long();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t direct_indirect_indexed_long_8() {
	uint32_t new_addr = direct_indirect_indexed_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t direct_indirect_indexed_long_16() {
	uint32_t new_addr = direct_indirect_indexed_long();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t absolute_8() {
	uint32_t new_addr = absolute();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t absolute_16() {
	uint32_t new_addr = absolute();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t absolute_indexed_x_8() {
	uint32_t new_addr = absolute_indexed_x();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t absolute_indexed_x_16() {
	uint32_t new_addr = absolute_indexed_x();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t absolute_indexed_y_8() {
	uint32_t new_addr = absolute_indexed_y();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t absolute_indexed_y_16() {
	uint32_t new_addr = absolute_indexed_y();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t absolute_long_8() {
	uint32_t new_addr = absolute_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t absolute_long_16() {
	uint32_t new_addr = absolute_long();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}
uint16_t absolute_long_32() {
	uint32_t new_addr = absolute_long();
	uint32_t data = get3Byte(access_address(new_addr));
	return data;
}
uint8_t absolute_indexed_long_8() {
	uint32_t new_addr = absolute_indexed_long();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t absolute_indexed_long_16() {
	uint32_t new_addr = absolute_indexed_long();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

uint8_t absolute_indirect_8() {}
uint16_t absolute_indirect_16() {}

uint8_t absolute_indirect_long_8() {}
uint16_t absolute_indirect_long_16() {}
uint32_t absolute_indirect_long_32() {}

uint8_t absolute_indexed_indirect_8() {}
uint16_t absolute_indexed_indirect_16() {}

uint8_t stack_relative_8() {
	uint16_t stack_addr = stack_relative();
	uint8_t data = accessSystemRam()[stack_addr];
	return data;
}
uint16_t stack_relative_16() {
	uint16_t stack_addr = stack_relative();
	uint16_t data = get2Byte(access_address(stack_addr));
	return data;
}

uint8_t stack_relative_indirect_indexed_8() {
	uint32_t new_addr = stack_relative_indirect_indexed();
	uint8_t data = (uint8_t)*access_address(new_addr);
	return data;
}
uint16_t stack_relative_indirect_indexed_16(){
	uint32_t new_addr = stack_relative_indirect_indexed();
	uint16_t data = get2Byte(access_address(new_addr));
	return data;
}

void push_to_stack_8(uint8_t val, uint32_t *stack) {
	uint8_t *local_pointer = access_address_from_bank(0x00, *stack);
	*local_pointer = val;
	stack_pointer -= 1;
	*stack -= 1;
}

void push_to_stack_32(uint32_t val, uint32_t *stack) {
	uint8_t* local_pointer = access_address(*stack);
	store4Byte(local_pointer, val);
	stack_pointer -= 4;
	*stack -= 4;
}

void push_to_stack_16(uint16_t val, uint32_t *stack) {
	uint8_t low_byte = (uint8_t) val;
	uint8_t high_byte = val >> 8;
	push_to_stack_8(high_byte, stack);
	push_to_stack_8(low_byte, stack);
}


uint8_t pull_from_stack_8(uint32_t *stack) {
	*stack += 1;
	uint32_t stack_val = *stack;
	uint8_t *local_pointer = access_address(stack_val);
	stack_pointer += 1;
	return *local_pointer;
}

uint32_t pull_from_stack_32(uint32_t *stack) {
	uint8_t* local_pointer = access_address(*stack);
	stack_pointer += 4;
	return get4Byte(local_pointer);
}

uint16_t pull_from_stack_16(uint32_t *stack) {
	uint8_t low_byte = pull_from_stack_8(stack);
	uint8_t high_byte = pull_from_stack_8(stack);
	uint16_t ret = high_byte;
	ret <<= 8;
	ret |= low_byte;
	return ret;
}



#pragma endregion

#pragma region ADC
void ADC_8(uint8_t toAdd) {
	accumulator = accumulator + toAdd + (1 & p_register);
}
void ADC_16(uint16_t toAdd) {
	accumulator = accumulator + toAdd + (1 & p_register);
}
//ADC(_dp_, X)	61	DP Indexed Indirect, X	NV— - ZC	2
void f61_ADC(){
	ADC_8(direct_indexed_indirect_8);
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

#pragma endregion

#pragma region AND
void AND_8(uint8_t val) {
	accumulator &= val;
}

void AND_16(uint16_t val) {
	accumulator &= val;
}

//AND(_dp, _X)	21	DP Indexed Indirect, X	N—–Z - 2
void f21_AND(){
	uint8_t data = direct_indexed_indirect_8();
	AND_8(data);
	program_counter += 2;
}

//AND sr, S	23	Stack Relative	N—–Z - 2
void f23_AND(){
	uint8_t data = stack_relative_8();
	AND_8(data);
	program_counter += 2;
}

//AND dp	25	Direct Page	N—–Z - 2
void f25_AND(){
	uint8_t data = direct_8();
	AND_8(data);
	program_counter += 2;
}

//AND[_dp_]	27	DP Indirect Long	N—–Z - 2
void f27_AND(){
	uint8_t data = direct_indirect_long_8();
	AND_8(data);
	program_counter += 2;
}

//AND #const	29	Immediate	N—–Z - 2
void f29_AND(){
	if (m_flag() == 0) {
		uint8_t data = immediate_16();
		AND_16(data);
		program_counter += 3;
	}
	else {
		uint8_t data = immediate_8();
		AND_8(data);
		program_counter += 2;
	}
	
}

//AND addr	2D	Absolute	N—–Z - 3
void f2D_AND(){
	uint8_t data = absolute_8();
	AND_8(data);
	program_counter += 3;
}

//AND long	2F	Absolute Long	N—–Z - 4
void f2F_AND(){
	uint8_t data = absolute_long_8();
	AND_8(data);
	program_counter += 4;
}

//AND(_dp_), Y	31	DP Indirect Indexed, Y	N—–Z - 2
void f31_AND(){
	uint8_t data = direct_indirect_indexed_8();
	AND_8(data);
	program_counter += 2;
}

//AND(_dp_)	32	DP Indirect	N—–Z - 2
void f32_AND(){
	uint8_t data = direct_indirect_8();
	AND_8(data);
	program_counter += 2;
}

//AND(_sr_, S), Y	33	SR Indirect Indexed, Y	N—–Z - 2
void f33_AND(){
	uint8_t data = stack_relative_indirect_indexed_8();
	AND_8(data);
	program_counter += 2;
}

//AND dp, X	35	DP Indexed, X	N—–Z - 2
void f35_AND(){
	uint8_t data = direct_indexed_x_8();
	AND_8(data);
	program_counter += 2;
}

//AND[_dp_], Y	37	DP Indirect Long Indexed, Y	N—–Z - 2
void f37_AND(){
	uint8_t data = direct_indirect_indexed_long_8();
	AND_8(data);
	program_counter += 2;
}

//AND addr, Y	39	Absolute Indexed, Y	N—–Z - 3
void f39_AND(){
	uint8_t data = absolute_indexed_y_8();
	AND_8(data);
	program_counter += 3;
}

//AND addr, X	3D	Absolute Indexed, X	N—–Z - 3
void f3D_AND(){
	uint8_t data = absolute_indexed_x();
	AND_8(data);
	program_counter += 3;
}

//AND long, X	3F	Absolute Long Indexed, X	N—–Z - 4
void f3F_AND(){
	uint8_t data = absolute_indexed_long_8();
	AND_8(data);
	program_counter += 4;
}

#pragma endregion

#pragma region ASL
void ASL_8(uint8_t *loc) {
	*loc = *loc << 1;
}

void ASL_16(uint8_t *loc){
	uint16_t val = get2Byte(loc);
	val = val << 1;
	*loc = (uint8_t)(val & 0x00FF);
	*(loc + 1) = (uint8_t)((val >> 8) & 0x00FF);

}
//ASL dp	6	Direct Page	N—–ZC	2
void f06_ASL(){
	uint16_t new_addr = direct();
	ASL_8(access_address_from_bank(data_bank_register >> 16, new_addr));
	program_counter += 2;
}

//ASL A	0A	Accumulator	N—–ZC	1
void f0A_ASL(){
	accumulator = accumulator << 1;
	program_counter += 1;
}

//ASL addr	0E	Absolute	N—–ZC	3
void f0E_ASL(){
	uint32_t new_addr = absolute();
	ASL_8(access_address(new_addr));
	program_counter += 3;
}

//ASL dp, X	16	DP Indexed, X	N—–ZC	2
void f16_ASL(){
	uint32_t new_addr = direct_indexed_x();
	ASL_8(access_address(new_addr));
	program_counter += 2;
}

//ASL addr, X	1E	Absolute Indexed, X	N—–ZC	3
void f1E_ASL(){
	uint16_t new_addr = absolute_indexed_x();
	ASL_8(access_address_from_bank(data_bank_register >> 16, new_addr));
	program_counter += 3;
}
#pragma endregion

//BCC nearlabel	90	Program Counter Relative		2
void f90_BCC(){
	if (CARRY_FLAG & p_register == 1)
		program_counter += 2;
	else {
		char adjust = (char) immediate_8();
		program_counter += adjust;
	}
	return;
}

//BCS nearlabel	B0	Program Counter Relative		2
void fB0_BCs(){
	if (CARRY_FLAG & p_register == 0)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//BEQ nearlabel	F0	Program Counter Relative		2
void fF0_BEQ(){
	if (ZERO_FLAG & p_register == 0)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

#pragma region BIT
void BIT_8(uint8_t val) {
	uint16_t anded = val & accumulator;
}
void BIT_16(uint16_t val) {

}
//BIT dp	24	Direct Page	NV— - Z - 2
void f24_BIT(){
	uint8_t val = direct_8();
	BIT_8(val);
	program_counter += 2;
}

//BIT addr	2C	Absolute	NV— - Z - 3
void f2C_BIT(){
	uint8_t val = absolute_8();
	BIT_8(val);
	program_counter += 3;
}

//BIT dp, X	34	DP Indexed, X	NV— - Z - 2
void f34_BIT(){
	uint8_t val = direct_indexed_x_8();
	BIT_8(val);
	program_counter += 2;
}

//BIT addr, X	3C	Absolute Indexed, X	NV— - Z - 3
void f3C_BIT(){
	uint8_t val = absolute_indexed_x_8();
	BIT_8(val);
	program_counter += 3;
}

//BIT #const	89	Immediate	——Z - 2
void f89_BIT(){
	uint8_t val = immediate_8();
	BIT_8(val);
	program_counter += 2;
}
#pragma endregion 

//BMI nearlabel	30	Program Counter Relative		2
void f30_BMI(){
	if (NEGATIVE_FLAG & p_register == 0)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//BNE nearlabel	D0	Program Counter Relative		2
void fD0_BNE(){
	if (ZERO_FLAG & p_register == 1)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//BPL nearlabel	10	Program Counter Relative		2
void f10_BPL(){
	if (NEGATIVE_FLAG & p_register == 1)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//BRA nearlabel	80	Program Counter Relative		2
void f80_BRA(){
	char adjust = (char)immediate_8();
	program_counter += adjust;
}

//BRK	0	Stack / Interrupt	— - DI–	28
void f00_BRK(){
	call_stack[cs_counter++] = program_counter = 2;
	program_counter = 0x00FFE6;
}

//BRL label	82	Program Counter Relative Long		3
void f82_BRL(){
	short adjust = (short)relative_long();
	program_counter += adjust;
}

//BVC nearlabel	50	Program Counter Relative		2
void f50_BVC(){
	if (OVERFLOW_FLAG & p_register == 1)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//BVS nearlabel	70	Program Counter Relative		2
void f70_BVS(){
	if (OVERFLOW_FLAG & p_register == 0)
		program_counter += 2;
	else {
		char adjust = (char)immediate_8();
		program_counter += adjust;
	}
	return;
}

//CLC	18	Implied	—— - C	1
void f18_CLC(){
	p_register = p_register & !(CARRY_FLAG);
}

//CLD	D8	Implied	— - D—	1
void fD8_CLD(){
	p_register = p_register & !(DECIMAL_FLAG);
}

//CLI	58	Implied	—–I–	1
void f58_CLI(){
	p_register = p_register & !(INTERRUPT_FLAG);
}

//CLV	B8	Implied	#NAME ? 1
void fB8_CLV(){
	p_register = p_register & !(OVERFLOW_FLAG);
}

#pragma region cmp

void CMP_8(uint8_t reg, uint8_t data) {
	uint8_t result = data - reg;
	if (result = 0)
		p_register |= ZERO_FLAG;
	else
		p_register &= !ZERO_FLAG;

	if (result & 0x8000 == 1)
		p_register |= NEGATIVE_FLAG;
	else
		p_register &= !NEGATIVE_FLAG;

	if(reg >= data)
		p_register |= CARRY_FLAG;
	else
		p_register &= !CARRY_FLAG;
}

void CMP_16(uint16_t reg, uint16_t data) {
	uint16_t result = data - reg;
	if (result = 0)
		p_register |= ZERO_FLAG;
	else
		p_register &= !ZERO_FLAG;

	if (result & 0x80000000 == 1)
		p_register |= NEGATIVE_FLAG;
	else
		p_register &= !NEGATIVE_FLAG;

	if (reg >= data)
		p_register |= CARRY_FLAG;
	else
		p_register &= !CARRY_FLAG;
}

//CMP(_dp, _X)	C1	DP Indexed Indirect, X	N—–ZC	2
void fC1_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indexed_indirect_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indexed_indirect_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP sr, S	C3	Stack Relative	N—–ZC	2
void fC3_CMP(){
	if (m_flag() == 0) {
		uint16_t val = stack_relative_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = stack_relative_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP dp	C5	Direct Page	N—–ZC	2
void fC5_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP[_dp_]	C7	DP Indirect Long	N—–ZC	2
void fC7_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indirect_long_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indirect_long_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP #const	C9	Immediate	N—–ZC	2
void fC9_CMP(){
	if (m_flag() == 0) {
		uint16_t val = immediate_16();
		CMP_16(accumulator, val);
		program_counter += 3;
	}
	else {
		uint8_t val = immediate_8();
		CMP_8(accumulator, val);
		program_counter += 2;
	}
	
}

//CMP addr	CD	Absolute	N—–ZC	3
void fCD_CMP(){
	if (m_flag() == 0) {
		uint16_t val = absolute_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = absolute_8();
		CMP_8(accumulator, val);
	}
	program_counter += 3;
}

//CMP long	CF	Absolute Long	N—–ZC	4
void fCF_CMP(){
	if (m_flag() == 0) {
		uint16_t val = absolute_long_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = absolute_long_8();
		CMP_8(accumulator, val);
	}
	program_counter += 4;
}

//CMP(_dp_), Y	D1	DP Indirect Indexed, Y	N—–ZC	2
void fD1_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indirect_indexed_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indirect_indexed_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP(_dp_)	D2	DP Indirect	N—–ZC	2
void fD2_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indirect_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indirect_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP(_sr_, S), Y	D3	SR Indirect Indexed, Y	N—–ZC	2
void fD3_CMP(){
	if (m_flag() == 0) {
		uint16_t val = stack_relative_indirect_indexed_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = stack_relative_indirect_indexed_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP dp, X	D5	DP Indexed, X	N—–ZC	2
void fD5_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indexed_x_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indexed_x_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP[_dp_], Y	D7	DP Indirect Long Indexed, Y	N—–ZC	2
void fD7_CMP(){
	if (m_flag() == 0) {
		uint16_t val = direct_indirect_indexed_long_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = direct_indirect_indexed_long_8();
		CMP_8(accumulator, val);
	}
	program_counter += 2;
}

//CMP addr, Y	D9	Absolute Indexed, Y	N—–ZC	3
void fD9_CMP(){
	if (m_flag() == 0) {
		uint16_t val = absolute_indexed_y_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = absolute_indexed_y_8();
		CMP_8(accumulator, val);
	}
	program_counter += 3;
}

//CMP addr, X	DD	Absolute Indexed, X	N—–ZC	3
void fDD_CMP(){
	if (m_flag() == 0) {
		uint16_t val = absolute_indexed_x_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = absolute_indexed_x_8();
		CMP_8(accumulator, val);
	}
	program_counter += 3;
}

//CMP long, X	DF	Absolute Long Indexed, X	N—–ZC	4
void fDF_CMP(){
	if (m_flag() == 0) {
		uint16_t val = absolute_indexed_long_16();
		CMP_16(accumulator, val);
	}
	else {
		uint8_t val = absolute_indexed_long_8();
		CMP_8(accumulator, val);
	}
	program_counter += 4;
}

//CPX #const	E0	Immediate	N—–ZC	210
void fE0_CPX(){
	if (x_flag() == 0) {
		uint16_t val = immediate_16();
		CMP_16(X, val);
		program_counter += 3;
	}
	else {
		uint8_t val = immediate_8();
		CMP_8(X, val);
		program_counter += 2;
	}
	
}

//CPX dp	E4	Direct Page	N—–ZC	2
void fE4_CPX(){
	if (x_flag() == 0) {
		uint16_t val = direct_16();
		CMP_16(X, val);
	}
	else {
		uint8_t val = direct_8();
		CMP_8(X, val);
	}
	program_counter += 2;
}

//CPX addr	EC	Absolute	N—–ZC	3
void fEC_CPX(){
	if (x_flag() == 0) {
		uint16_t val = absolute_16();
		CMP_16(X, val);
	}
	else {
		uint8_t val = absolute_8();
		CMP_8(X, val);
	}
	program_counter += 2;
}

//CPY #const	C0	Immediate	N—–ZC	2
void fC0_CPY(){
	if (x_flag() == 0) {
		uint16_t val = immediate_16();
		CMP_16(Y, val);
		program_counter += 3;
	}
	else {
		uint8_t val = immediate_8();
		CMP_8(Y, val);
		program_counter += 2;
	}
	
}

//CPY dp	C4	Direct Page	N—–ZC	2
void fC4_CPY(){
	if (x_flag() == 0) {
		uint16_t val = direct_16();
		CMP_16(Y, val);
	}
	else {
		uint8_t val = direct_8();
		CMP_8(Y, val);
	}
	program_counter += 2;
}

//CPY addr	CC	Absolute	N—–ZC	3
void fCC_CPY(){
	if (x_flag() == 0) {
		uint16_t val = absolute_16();
		CMP_16(Y, val);
	}
	else {
		uint8_t val = absolute_8();
		CMP_8(Y, val);
	}
	program_counter += 2;
}
#pragma endregion

//COP const	2	Stack / Interrupt	— - DI–	2
void f02_COP() {
	call_stack[cs_counter++] = program_counter = 2;
	program_counter = 0x00FFE4;
}

#pragma region INC_DEC

void DEC_8(uint8_t* local_address) {
	uint8_t val  = *local_address - 1;
	*local_address = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
}

void DEC_16(uint8_t* local_address) {
	uint16_t val = get2Byte(local_address) - 1;
	store2Byte_local(local_address, val);
	set_p_register_16(val, NEGATIVE_FLAG | ZERO_FLAG);
}

void DEC(uint8_t* local_address) {
	if (m_flag() == 0)
		DEC_16(local_address);
	else
		DEC_8(local_address);
}

//DEC A	3A	Accumulator	N—–Z - 1
void f3A_DEC(){
	DEC(&accumulator);
	program_counter += 1;
}

//DEC dp	C6	Direct Page	N—–Z - 2
void fC6_DEC(){
	uint16_t snes_addr = direct();
	uint8_t* local_addr = access_address_from_bank(data_bank_register, snes_addr);
	DEC(local_addr);
	program_counter += 2;
}

//DEC addr	CE	Absolute	N—–Z - 3
void fCE_DEC(){
	uint32_t snes_addr = absolute();
	uint8_t* local_addr = access_address(snes_addr);
	DEC(local_addr);
	program_counter += 3;
}

//DEC dp, X	D6	DP Indexed, X	N—–Z - 2
void fD6_DEC(){
	uint32_t snes_addr = direct_indexed_x();
	uint8_t* local_addr = access_address(snes_addr);
	DEC(local_addr);
	program_counter += 2;
}

//DEC addr, X	DE	Absolute Indexed, X	N—–Z - 3
void fDE_DEC(){
	uint16_t snes_addr = absolute_indexed_x();
	uint8_t* local_addr = access_address_from_bank(data_bank_register, snes_addr);
	DEC(local_addr);
	program_counter += 3;
}

//DEX	CA	Implied	N—–Z - 1
void fCA_DEX(){
	DEC(&X);
	program_counter += 1;
}

//DEY	88	Implied	N—–Z - 1
void f88_DEY(){
	DEC(&Y);
	program_counter += 1;
}

void INC_8(uint8_t* local_address) {
	uint8_t val = *local_address + 1;
	*local_address = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
}

void INC_16(uint8_t* local_address) {
	uint16_t val = get2Byte(local_address) + 1;
	store2Byte_local(local_address, val);
	set_p_register_16(val, NEGATIVE_FLAG | ZERO_FLAG);
}

void INC(uint8_t* local_address) {
	if (m_flag() == 0)
		INC_16(local_address);
	else
		INC_8(local_address);
}

//INC A	1A	Accumulator	N—–Z - 1
void f1A_INC() {
	INC(&accumulator);
	program_counter += 1;
}

//INC dp	E6	Direct Page	N—–Z - 2
void fE6_INC() {
	uint16_t snes_addr = direct();
	uint8_t* local_addr = access_address_from_bank(data_bank_register, snes_addr);
	INC(local_addr);
	program_counter += 2;
}

//INC addr	EE	Absolute	N—–Z - 3
void fEE_INC() {
	uint32_t snes_addr = absolute();
	uint8_t* local_addr = access_address(snes_addr);
	INC(local_addr);
	program_counter += 3;
}

//INC dp, X	F6	DP Indexed, X	N—–Z - 2
void fF6_INC() {
	uint32_t snes_addr = direct_indexed_x();
	uint8_t* local_addr = access_address(snes_addr);
	INC(local_addr);
	program_counter += 2;
}

//INC addr, X	FE	Absolute Indexed, X	N—–Z - 3
void fFE_INC() {
	uint16_t snes_addr = absolute_indexed_x();
	uint8_t* local_addr = access_address_from_bank(data_bank_register, snes_addr);
	INC(local_addr);
	program_counter += 3;
}

//INX	E8	Implied	N—–Z - 1
void fE8_INX() {
	INC(&X);
	program_counter += 1;
}

//INY	C8	Implied	N—–Z - 1
void fC8_INY() {
	INC(&Y);
	program_counter += 1;
}
#pragma endregion

#pragma region EOR
void EOR_8(uint8_t val) {
	accumulator ^= val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
}
void EOR_16(uint16_t val) {
	accumulator ^= val;
	set_p_register_16(val, NEGATIVE_FLAG | ZERO_FLAG);
}

//EOR(_dp, _X)	41	DP Indexed Indirect, X	N—–Z - 2
void f41_EOR(){
	if(m_flag() == 1) {
		EOR_8(direct_indexed_indirect_8());
	}
	else {
		EOR_16(direct_indexed_indirect_16());
	}

	program_counter += 2;
}

//EOR sr, S	43	Stack Relative	N—–Z - 2
void f43_EOR(){
	if (m_flag() == 1) {
		EOR_8(stack_relative_8());
	}
	else {
		EOR_16(stack_relative_16());
	}

	program_counter += 2;
}

//EOR dp	45	Direct Page	N—–Z - 2
void f45_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_8());
	}
	else {
		EOR_16(direct_16());
	}

	program_counter += 2;
}

//EOR[_dp_]	47	DP Indirect Long	N—–Z - 2
void f47_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_indirect_long_8());
	}
	else {
		EOR_16(direct_indirect_long_16());
	}

	program_counter += 2;
}

//EOR #const	49	Immediate	N—–Z - 2
void f49_EOR(){
	if (m_flag() == 1) {
		EOR_8(immediate_8());
		program_counter += 2;
	}
	else {
		EOR_16(immediate_16());
		program_counter += 3;
	}
}

//EOR addr	4D	Absolute	N—–Z - 3
void f4D_EOR(){
	if (m_flag() == 1) {
		EOR_8(absolute_8());
	}
	else {
		EOR_16(absolute_16());
	}

	program_counter += 3;
}

//EOR long	4F	Absolute Long	N—–Z - 4
void f4F_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_indexed_indirect_8());
	}
	else {
		EOR_16(direct_indexed_indirect_16());
	}

	program_counter += 4;
}

//EOR(_dp_), Y	51	DP Indirect Indexed, Y	N—–Z - 2
void f51_EOR(){
	if (m_flag() == 1) {
		EOR_8(absolute_long_8());
	}
	else {
		EOR_16(absolute_long_16());
	}

	program_counter += 2;
}

//EOR(_dp_)	52	DP Indirect	N—–Z - 2
void f52_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_indirect_8());
	}
	else {
		EOR_16(direct_indirect_16());
	}

	program_counter += 2;
}

//EOR(_sr_, S), Y	53	SR Indirect Indexed, Y	N—–Z - 2
void f53_EOR(){
	if (m_flag() == 1) {
		EOR_8(stack_relative_indirect_indexed_8());
	}
	else {
		EOR_16(stack_relative_indirect_indexed_16());
	}

	program_counter += 2;
}

//EOR dp, X	55	DP Indexed, X	N—–Z - 2
void f55_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_indexed_x_8());
	}
	else {
		EOR_16(direct_indexed_x_16());
	}

	program_counter += 2;
}

//EOR[_dp_], Y	57	DP Indirect Long Indexed, Y	N—–Z - 2
void f57_EOR(){
	if (m_flag() == 1) {
		EOR_8(direct_indirect_indexed_long_8());
	}
	else {
		EOR_16(direct_indirect_indexed_long_16());
	}

	program_counter += 2;
}

//EOR addr, Y	59	Absolute Indexed, Y	N—–Z - 3
void f59_EOR(){
	if (m_flag() == 1) {
		EOR_8(absolute_indexed_y_8());
	}
	else {
		EOR_16(absolute_indexed_y_16());
	}

	program_counter += 3;
}

//EOR addr, X	5D	Absolute Indexed, X	N—–Z - 3
void f5D_EOR(){
	if (m_flag() == 1) {
		EOR_8(absolute_indexed_x_8());
	}
	else {
		EOR_16(absolute_indexed_x_16());
	}

	program_counter += 3;
}

//EOR long, X	5F	Absolute Long Indexed, X	N—–Z - 4
void f5F_EOR(){
	if (m_flag() == 1) {
		EOR_8(absolute_indexed_long_8());
	}
	else {
		EOR_16(absolute_indexed_long_16());
	}

	program_counter += 4;
}
#pragma endregion

#pragma region JMP
//JMP addr	4C	Absolute		3
void f4C_JMP(){
	uint32_t operand = absolute_long_16();
	program_counter = program_bank_register;
	program_counter << 16;
	program_counter |= operand;
}

//JMP long	5C	Absolute Long		4
void f5C_JMP(){
	uint32_t operand = absolute_long();
	program_counter = operand;
}

//JMP(_addr_)	6C	Absolute Indirect		3
void f6C_JMP(){
	uint16_t operand = absolute_indirect_16();
	program_counter = program_bank_register;
	program_counter << 16;
	program_counter |= operand;
}

//JMP(_addr, X_)	7C	Absolute Indexed Indirect		3
void f7C_JMP(){
	uint16_t operand = absolute_indexed_indirect_16();
	program_counter = program_bank_register;
	program_counter << 16;
	program_counter |= operand;
}

//JMP[addr]	DC	Absolute Indirect Long		3
void fDC_JMP(){
	uint32_t operand = absolute_indirect_long_32();
	program_counter = operand;
}

//JSR addr	20	Absolute		3
void f20_JSR(){
	uint16_t operand = absolute();
	push_to_stack_16(program_counter + 2, &stack);
	program_counter &= 0xFFFF0000;
	program_counter |= operand;
}

//JSR or JSL long	22	Absolute Long		4
void f22_JSR(){
	uint32_t operand = absolute_long();
	uint8_t pbr = program_counter >> 16;
	push_to_stack_8(pbr, &stack);
	uint16_t addr = program_counter;
	push_to_stack_16(addr + 3, &stack);
	program_counter = operand;

}

//JSR(addr, X))	FC	Absolute Indexed Indirect		3
void fFC_JSR(){
	uint16_t operand = absolute_indexed_indirect_16();
	push_to_stack_16(program_counter + 2, &stack);
	program_counter = program_bank_register;
	program_counter << 16;
	program_counter |= operand;
}
#pragma endregion

#pragma region LD
//LDA(_dp, _X)	A1	DP Indexed Indirect, X	N—–Z - 2
void fA1_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indexed_indirect_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indexed_indirect_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		
	}
	program_counter += 2;
}

//LDA sr, S	A3	Stack Relative	N—–Z - 2
void fA3_LDA(){
	if (m_flag() == 0) {
		accumulator = stack_relative_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = stack_relative_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2; 
}

//LDA dp	A5	Direct Page	N—–Z - 2
void fA5_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA[_dp_]	A7	DP Indirect Long	N—–Z - 2
void fA7_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indirect_long_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indirect_long_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA #const	A9	Immediate	N—–Z - 2
void fA9_LDA(){
	if (m_flag() == 0) {
		accumulator = immediate_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 3;
	}
	else {
		accumulator &= 0xFF00;
		accumulator |= immediate_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 2;
	}
	
}

//LDA addr	AD	Absolute	N—–Z - 3
void fAD_LDA(){
	if (m_flag() == 0) {
		accumulator = absolute_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = absolute_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}

//LDA long	AF	Absolute Long	N—–Z - 4
void fAF_LDA(){
	if (m_flag() == 0) {
		accumulator = absolute_long_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = absolute_long_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 4;
}

//LDA(_dp_), Y	B1	DP Indirect Indexed, Y	N—–Z - 2
void fB1_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indexed_indirect_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indexed_indirect_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA(_dp_)	B2	DP Indirect	N—–Z - 2
void fB2_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indirect_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indirect_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA(_sr_, S), Y	B3	SR Indirect Indexed, Y	N—–Z - 2
void fB3_LDA(){
	if (m_flag() == 0) {
		accumulator = stack_relative_indirect_indexed_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = stack_relative_indirect_indexed_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA dp, X	B5	DP Indexed, X	N—–Z - 2
void fB5_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indexed_x_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indexed_x_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA[_dp_], Y	B7	DP Indirect Long Indexed, Y	N—–Z - 2
void fB7_LDA(){
	if (m_flag() == 0) {
		accumulator = direct_indirect_indexed_long_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = direct_indirect_indexed_long_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDA addr, Y	B9	Absolute Indexed, Y	N—–Z - 3
void fB9_LDA(){
	if (m_flag() == 0) {
		accumulator = absolute_indexed_y_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = absolute_indexed_y_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}

//LDA addr, X	BD	Absolute Indexed, X	N—–Z - 3
void fBD_LDA(){
	if (m_flag() == 0) {
		accumulator = absolute_indexed_x_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = absolute_indexed_x_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}

//LDA long, X	BF	Absolute Long Indexed, X	N—–Z - 4
void fBF_LDA(){
	if (m_flag() == 0) {
		accumulator = absolute_indexed_long_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = absolute_indexed_long_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 4;
}

//LDX #const	A2	Immediate	N—–Z - 212
void fA2_LDX(){
	if (x_flag() == 0) {
		X = immediate_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 3;
	}
	else {
		X = immediate_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 2;
	}
	
}

//LDX dp	A6	Direct Page	N—–Z - 2
void fA6_LDX(){
	if (x_flag() == 0) {
		X = direct_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		X = direct_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDX addr	AE	Absolute	N—–Z - 3
void fAE_LDX(){
	if (x_flag() == 0) {
		X = absolute_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		X = absolute_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}

//LDX dp, Y	B6	DP Indexed, Y	N—–Z - 2
void fB6_LDX(){
	if (x_flag() == 0) {
		X = direct_indexed_y_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		X = direct_indexed_y_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDX addr, Y	BE	Absolute Indexed, Y	N—–Z - 3
void fBE_LDX(){
	if (x_flag() == 0) {
		X = absolute_indexed_y_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		X = absolute_indexed_y_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDY #const	A0	Immediate	N—–Z - 2
void fA0_LDY(){
	if (x_flag() == 0) {
		Y = immediate_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 3;
	}
	else {
		Y = immediate_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
		program_counter += 2;

	}
}

//LDY dp	A4	Direct Page	N—–Z - 2
void fA4_LDY(){
	if (x_flag() == 0) {
		Y = direct_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		Y = direct_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDY addr	AC	Absolute	N—–Z - 3
void fAC_LDY(){
	if (x_flag() == 0) {
		Y = absolute_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		Y = absolute_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}

//LDY dp, X	B4	DP Indexed, X	N—–Z - 2
void fB4_LDY(){
	if (x_flag() == 0) {
		Y = direct_indexed_x_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		Y = direct_indexed_x_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 2;
}

//LDY addr, X	BC	Absolute Indexed, X	N—–Z - 3
void fBC_LDY(){
	if (x_flag() == 0) {
		Y = absolute_indexed_x_16();
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		Y = absolute_indexed_x_8();
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);

	}
	program_counter += 3;
}
#pragma endregion

#pragma region LSR
void LSR_8(uint8_t *local_address) {
	uint8_t val = *local_address;
	p_register |= CARRY_FLAG & accumulator;
	val >> 1;
}
void LSR_16(uint8_t *local_address) {
	uint16_t val = get2Byte(local_address);
	p_register |= CARRY_FLAG & accumulator;
	val >> 1;
}

//LSR dp	46	Direct Page	N—–ZC	2
void f46_LSR(){
	if (m_flag() == 0) {
		LSR_16(direct());
	}
	else {
		LSR_8(direct());
	}
	
	program_counter += 2;
}

//LSR A	4A	Accumulator	N—–ZC	1
void f4A_LSR(){
	p_register |= CARRY_FLAG & accumulator;
	accumulator = accumulator >> 1;
	program_counter += 1;
}

//LSR addr	4E	Absolute	N—–ZC	3
void f4E_LSR(){
	if (m_flag() == 0) {
		LSR_16(absolute());
	}
	else {
		LSR_8(absolute());
	}

	program_counter += 3;
}

//LSR dp, X	56	DP Indexed, X	N—–ZC	2
void f56_LSR(){
	if (m_flag() == 0) {
		LSR_16(direct_indexed_x());
	}
	else {
		LSR_8(direct_indexed_x());
	}

	program_counter += 2;
}

//LSR addr, X	5E	Absolute Indexed, X	N—–ZC	3
void f5E_LSR(){
	if (m_flag() == 0) {
		LSR_16(absolute_indexed_x());
	}
	else {
		LSR_8(absolute_indexed_x());
	}

	program_counter += 3;
}
#pragma endregion

//MVN srcbk, destbk	54	Block Move		3
void f54_MVN(){
	uint16_t bytes_to_move = accumulator + 1;
	uint8_t src_blk = immediate_8();
	uint16_t src_addr = X;
	program_counter++;
	uint8_t dest_blk = immediate_8();
	uint16_t dest_addr = Y;
	
	uint8_t *src_addr_local = access_address_from_bank(src_blk, src_addr);
	uint8_t *dest_addr_local = access_address_from_bank(dest_blk, dest_addr);

	memcpy(dest_addr_local, src_addr_local, bytes_to_move);
	accumulator = 0xFFFF;
	X += bytes_to_move;
	Y += bytes_to_move;

	program_counter += 2;
}

//MVP srcbk, destbk	44	Block Move		3
void f44_MVP(){
	uint16_t bytes_to_move = accumulator + 1;
	X -= bytes_to_move;
	Y -= bytes_to_move;

	uint8_t src_blk = immediate_8();
	uint16_t src_addr = X;
	program_counter++;
	uint8_t dest_blk = immediate_8();
	uint16_t dest_addr = Y;

	uint8_t *src_addr_local = access_address_from_bank(src_blk, src_addr);
	uint8_t *dest_addr_local = access_address_from_bank(dest_blk, dest_addr);

	memcpy(dest_addr_local, src_addr_local, bytes_to_move);
	accumulator = 0xFFFF;
	

	program_counter += 2;
}

//NOP	EA	Implied		1
void fEA_NOP(){
	program_counter += 1;
}

#pragma region ORA

void ORA_8(uint8_t val) {
	accumulator |= val;
	set_p_register_8(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
}
void ORA_16(uint16_t val) {
	accumulator |= val;
	set_p_register_16(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
}

//ORA(_dp, _X)	1	DP Indexed Indirect, X	N—–Z - 2
void f01_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indexed_indirect_16());
	}
	else {
		ORA_16(direct_indexed_indirect_8());
	}
	program_counter += 2;
}

//ORA sr, S	3	Stack Relative	N—–Z - 2
void f03_ORA(){
	if (m_flag() == 0) {
		ORA_8(stack_relative_16());
	}
	else {
		ORA_16(stack_relative_8());
	}
	program_counter += 2;
}

//ORA dp	5	Direct Page	N—–Z - 2
void f05_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_16());
	}
	else {
		ORA_16(direct_8());
	}
	program_counter += 2;
}

//ORA[_dp_]	7	DP Indirect Long	N—–Z - 2
void f07_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indirect_long_16());
	}
	else {
		ORA_16(direct_indirect_long_8());
	}
	program_counter += 2;
}

//ORA #const	9	Immediate	N—–Z - 2
void f09_ORA(){
	if (m_flag() == 0) {
		ORA_8(immediate_16());
		program_counter += 3;
	}
	else {
		ORA_16(immediate_8());
		program_counter += 2;
	}
	
}

//ORA addr	0D	Absolute	N—–Z - 3
void f0D_ORA(){
	if (m_flag() == 0) {
		ORA_8(absolute_16());
	}
	else {
		ORA_16(absolute_8());
	}
	program_counter += 3;
}

//ORA long	0F	Absolute Long	N—–Z - 4
void f0F_ORA(){
	if (m_flag() == 0) {
		ORA_8(absolute_long_16());
	}
	else {
		ORA_16(absolute_long_8());
	}
	program_counter += 4;
}

//ORA(_dp_), Y	11	DP Indirect Indexed, Y	N—–Z - 2
void f11_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indirect_indexed_16());
	}
	else {
		ORA_16(direct_indirect_indexed_8());
	}
	program_counter += 2;
}

//ORA(_dp_)	12	DP Indirect	N—–Z - 2
void f12_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indirect_16());
	}
	else {
		ORA_16(direct_indirect_8());
	}
	program_counter += 2;
}

//ORA(_sr_, S), Y	13	SR Indirect Indexed, Y	N—–Z - 2
void f13_ORA(){
	if (m_flag() == 0) {
		ORA_8(stack_relative_indirect_indexed_16());
	}
	else {
		ORA_16(stack_relative_indirect_indexed_8());
	}
	program_counter += 2;
}

//ORA dp, X	15	DP Indexed, X	N—–Z - 2
void f15_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indexed_x_16());
	}
	else {
		ORA_16(direct_indexed_x_8());
	}
	program_counter += 2;
}

//ORA[_dp_], Y	17	DP Indirect Long Indexed, Y	N—–Z - 2
void f17_ORA(){
	if (m_flag() == 0) {
		ORA_8(direct_indirect_long_16());
	}
	else {
		ORA_16(direct_indirect_long_8());
	}
	program_counter += 2;
}

//ORA addr, Y	19	Absolute Indexed, Y	N—–Z - 3
void f19_ORA(){
	if (m_flag() == 0) {
		ORA_8(absolute_indexed_y_16());
	}
	else {
		ORA_16(absolute_indexed_y_8());
	}
	program_counter += 3;
}

//ORA addr, X	1D	Absolute Indexed, X	N—–Z - 3
void f1D_ORA(){
	if (m_flag() == 0) {
		ORA_8(absolute_indexed_x_16());
	}
	else {
		ORA_16(absolute_indexed_x_8());
	}
	program_counter += 3;
}

//ORA long, X	1F	Absolute Long Indexed, X	N—–Z - 4
void f1F_ORA(){
	if (m_flag() == 0) {
		ORA_8(absolute_indexed_long_16());
	}
	else {
		ORA_16(absolute_indexed_long_8());
	}
	program_counter += 4;
}
#pragma endregion

#pragma region stack
//PEA addr	F4	Stack(Absolute)		3
void fF4_PEA(){
	uint16_t operand = immediate_16();
	push_to_stack_16(operand, &stack);
	program_counter += 3;
}

//PEI(dp)	D4	Stack(DP Indirect)		2
void fD4_PEI(){
	uint16_t operand = immediate_8();
	push_to_stack_16(operand, &stack);
	program_counter += 2;
}

//PER label	62	Stack(PC Relative Long)		3
void f62_PER(){
	uint16_t operand = absolute_16() + program_counter;
	push_to_stack_16(operand, &stack);
	program_counter += 3;
}

//PHA	48	Stack(Push)		1
void f48_PHA(){
	if(m_flag() == 0)
		push_to_stack_16(accumulator, &stack);
	else
		push_to_stack_8(accumulator, &stack);

	program_counter += 1;
}

//PHB	8B	Stack(Push)		1
void f8B_PHB(){
	push_to_stack_8(data_bank_register, &stack);
	program_counter++;
}

//PHD	0B	Stack(Push)		1
void f0B_PHD(){
	push_to_stack_16(direct_page, &stack);
	program_counter++;
}

//PHK	4B	Stack(Push)		1
void f4B_PHK(){
	push_to_stack_8((uint8_t) (program_counter >> 16), &stack);
	program_counter++;
}

//PHP	8	Stack(Push)		1
void f08_PHP(){
	push_to_stack_16(p_register, &stack);
	program_counter++;
}

//PHX	DA	Stack(Push)		1
void fDA_PHX(){
	if (x_flag() == 0)
		push_to_stack_16(X, &stack);
	else
		push_to_stack_8(X, &stack);

	program_counter += 1;
}

//PHY	5A	Stack(Push)		1
void f5A_PHY(){
	if (x_flag() == 0)
		push_to_stack_16(Y, &stack);
	else
		push_to_stack_8(Y, &stack);

	program_counter += 1;
}

//PLA	68	Stack(Pull)	N—–Z - 1
void f68_PLA(){
	if (m_flag() == 0) {
		accumulator = pull_from_stack_16(&stack);
		set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		accumulator = pull_from_stack_8(&stack);
		set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
	}

	program_counter += 1;
}

//PLB	AB	Stack(Pull)	N—–Z - 1
void fAB_PLB(){
	uint8_t val = pull_from_stack_8(&stack);
	data_bank_register = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
	program_counter += 1;
}

//PLD	2B	Stack(Pull)	N—–Z - 1
void f2B_PLD(){
	uint16_t val = pull_from_stack_16(&stack);
	direct_page = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
	program_counter += 1;
}

//PLP	28	Stack(Pull)	N—–Z - 1
void f28_PLP(){
	p_register = pull_from_stack_16(&stack);
	program_counter += 1;
}

//PLX	FA	Stack(Pull)	N—–Z - 1
void fFA_PLX(){
	if (x_flag() == 0) {
		X = pull_from_stack_16(&stack);
		set_p_register_16(X, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		X = pull_from_stack_8(&stack);
		set_p_register_8(X, NEGATIVE_FLAG | ZERO_FLAG);
	}

	program_counter += 1;
}

//PLY	7A	Stack(Pull)	N—–Z - 1
void f7A_PLY(){
	if (x_flag() == 0) {
		Y = pull_from_stack_16(&stack);
		set_p_register_16(Y, NEGATIVE_FLAG | ZERO_FLAG);
	}
	else {
		Y = pull_from_stack_8(&stack);
		set_p_register_8(Y, NEGATIVE_FLAG | ZERO_FLAG);
	}

	program_counter += 1;
}
#pragma endregion

#pragma region rot

void ROL_8(uint8_t* addr) {
	uint8_t val = *addr;
	uint8_t c_flag = p_register & CARRY_FLAG;
	uint8_t new_c_flag = val & 0x8000;
	p_register &= !CARRY_FLAG | new_c_flag;
	val <<= 1;
	val |= c_flag;
	*addr = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
}
void ROL_16(uint8_t* addr) {
	uint16_t val = get2Byte(addr);
	uint8_t c_flag = p_register & CARRY_FLAG;
	uint8_t new_c_flag = val & 0x80000000;
	p_register &= !CARRY_FLAG | new_c_flag;
	val <<= 1;
	val |= c_flag;
	store2Byte_local(addr, val);
	set_p_register_16(val, NEGATIVE_FLAG | ZERO_FLAG);
}

void ROR_8(uint8_t* addr) {
	uint8_t val = *addr;
	uint8_t c_flag = p_register & CARRY_FLAG;
	uint8_t new_c_flag = val & CARRY_FLAG;
	p_register &= !CARRY_FLAG | new_c_flag;
	c_flag <<= 7;
	val >>= 1;
	val |= c_flag;
	*addr = val;
	set_p_register_8(val, NEGATIVE_FLAG | ZERO_FLAG);
}
void ROR_16(uint8_t* addr) {
	uint16_t val = get2Byte(addr);
	uint16_t c_flag = p_register & CARRY_FLAG;
	uint8_t new_c_flag = val & CARRY_FLAG;
	p_register &= !CARRY_FLAG | new_c_flag;
	c_flag <<= 15;
	val >>= 1;
	val |= c_flag;
	store2Byte_local(addr, val);
	set_p_register_16(val, NEGATIVE_FLAG | ZERO_FLAG);
}

//ROL dp	26	Direct Page	N—–ZC	2
void f26_ROL(){
	if (m_flag() == 0) {
		ROL_16(access_address_from_bank(data_bank_register, direct()));
	}
	else {
		ROL_8(access_address_from_bank(data_bank_register, direct()));
	}
	program_counter += 2;
}

//ROL A	2A	Accumulator	N—–ZC	1
void f2A_ROL(){
	if (m_flag() == 0) {
		ROL_16(&accumulator);
	}
	else {
		ROL_8(&accumulator);
	}
	program_counter += 1;
}

//ROL addr	2E	Absolute	N—–ZC	3
void f2E_ROL(){
	if (m_flag() == 0) {
		ROL_16(access_address(absolute()));
	}
	else {
		ROL_8(access_address(absolute()));
	}
	program_counter += 3;
}

//ROL dp, X	36	DP Indexed, X	N—–ZC	2
void f36_ROL(){
	if (m_flag() == 0) {
		ROL_16(access_address(direct_indexed_x()));
	}
	else {
		ROL_8(access_address(direct_indexed_x()));
	}
	program_counter += 3;
}

//ROL addr, X	3E	Absolute Indexed, X	N—–ZC	3
void f3E_ROL(){
	if (m_flag() == 0) {
		ROL_16(access_address_from_bank(data_bank_register, absolute_indexed_x()));
	}
	else {
		ROL_8(access_address_from_bank(data_bank_register, absolute_indexed_x()));
	}
	program_counter += 3;
}

//ROR dp	66	Direct Page	N—–ZC	2
void f66_ROR(){
	if (m_flag() == 0) {
		ROR_16(access_address_from_bank(data_bank_register, direct()));
	}
	else {
		ROR_8(access_address_from_bank(data_bank_register, direct()));
	}
	program_counter += 2;
}

//ROR A	6A	Accumulator	N—–ZC	1
void f6A_ROR(){
	if (m_flag() == 0) {
		ROR_16(&accumulator);
	}
	else {
		ROR_8(&accumulator);
	}
	program_counter += 1;
}

//ROR addr	6E	Absolute	N—–ZC	3
void f6E_ROR(){
	if (m_flag() == 0) {
		ROR_16(access_address(absolute()));
	}
	else {
		ROR_8(access_address(absolute()));
	}
	program_counter += 3;
}

//ROR dp, X	76	DP Indexed, X	N—–ZC	2
void f76_ROR(){
	if (m_flag() == 0) {
		ROR_16(access_address(direct_indexed_x()));
	}
	else {
		ROR_8(access_address(direct_indexed_x()));
	}
	program_counter += 3;
}

//ROR addr, X	7E	Absolute Indexed, X	N—–ZC	3
void f7E_ROR(){
	if (m_flag() == 0) {
		ROR_16(access_address_from_bank(data_bank_register, absolute_indexed_x()));
	}
	else {
		ROR_8(access_address_from_bank(data_bank_register, absolute_indexed_x()));
	}
	program_counter += 3;
}

#pragma endregion

#pragma region returns
//RTI	40	Stack(RTI)	NVMXDIZC	1
void f40_RTI(){
	p_register = pull_from_stack_16(&stack);
	if (m_flag() == 0) {
		uint16_t pc_addr = pull_from_stack_16(&stack);
		uint32_t bank = pull_from_stack_8(&stack);
		bank <<= 16;
		bank |= pc_addr;
		program_counter = bank;
	}
	else {
		uint16_t pc_addr = pull_from_stack_16(&stack);
		uint32_t bank = program_bank_register;
		bank |= pc_addr;
		program_counter = bank;
	}
}

//RTL	6B	Stack(RTL)		1
void f6B_RTL(){
	uint16_t addr = pull_from_stack_16(&stack) + 1;
	uint32_t bank = pull_from_stack_8(&stack);
	uint32_t pc_addr = bank << 16 | (uint32_t)addr;
	program_counter = pc_addr;
}

//RTS	60	Stack(RTS)		1
void f60_RTS(){
	uint16_t addr = pull_from_stack_16(&stack) + 1;
	program_counter = program_bank_register | (uint32_t)addr;
}

#pragma endregion

#pragma region SBC
void SBC_8(uint8_t val) {
	accumulator = accumulator - val + (p_register & CARRY_FLAG);
	set_p_register_8(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
}

void SBC_16(uint16_t val) {
	accumulator = accumulator - val + (p_register & CARRY_FLAG);
	set_p_register_16(accumulator, NEGATIVE_FLAG | ZERO_FLAG);
}

//SBC(_dp, _X)	E1	DP Indexed Indirect, X	NV— - ZC	2
void fE1_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indexed_indirect_16());
	}
	else {
		SBC_8(direct_indexed_indirect_8());
	}
	program_counter += 2;

}

//SBC sr, S	E3	Stack Relative	NV— - ZC	2
void fE3_SBC(){
	if (m_flag() == 0) {
		SBC_16(stack_relative_16());
	}
	else {
		SBC_8(stack_relative_8());
	}
	program_counter += 2;
}

//SBC dp	E5	Direct Page	NV— - ZC	2
void fE5_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_16());
	}
	else {
		SBC_8(direct_8());
	}
	program_counter += 2;
}

//SBC[_dp_]	E7	DP Indirect Long	NV— - ZC	2
void fE7_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indirect_long_16());
	}
	else {
		SBC_8(direct_indirect_long_8());
	}
	program_counter += 2;
}

//SBC #const	E9	Immediate	NV— - ZC	2
void fE9_SBC(){
	if (m_flag() == 0) {
		SBC_16(immediate_16());
		program_counter += 3;
	}
	else {
		SBC_8(immediate_8());
		program_counter += 2;
	}
	
}

//SBC addr	ED	Absolute	NV— - ZC	3
void fED_SBC(){
	if (m_flag() == 0) {
		SBC_16(absolute_16());
	}
	else {
		SBC_8(absolute_8());
	}
	program_counter += 3;
}

//SBC long	EF	Absolute Long	NV— - ZC	4
void fEF_SBC(){
	if (m_flag() == 0) {
		SBC_16(absolute_long_16());
	}
	else {
		SBC_8(absolute_long_8());
	}
	program_counter += 4;
}

//SBC(_dp_), Y	F1	DP Indirect Indexed, Y	NV— - ZC	2
void fF1_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indirect_indexed_16());
	}
	else {
		SBC_8(direct_indirect_indexed_8());
	}
	program_counter += 2;
}

//SBC(_dp_)	F2	DP Indirect	NV— - ZC	2
void fF2_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indirect_16());
	}
	else {
		SBC_8(direct_indirect_8());
	}
	program_counter += 2;
}

//SBC(_sr_, S), Y	F3	SR Indirect Indexed, Y	NV— - ZC	2
void fF3_SBC(){
	if (m_flag() == 0) {
		SBC_16(stack_relative_indirect_indexed_16());
	}
	else {
		SBC_8(stack_relative_indirect_indexed_8());
	}
	program_counter += 2;
}

//SBC dp, X	F5	DP Indexed, X	NV— - ZC	2
void fF5_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indexed_x_16());
	}
	else {
		SBC_8(direct_indexed_x_8());
	}
	program_counter += 2;
}

//SBC[_dp_], Y	F7	DP Indirect Long Indexed, Y	NV— - ZC	2
void fF7_SBC(){
	if (m_flag() == 0) {
		SBC_16(direct_indirect_indexed_long_16());
	}
	else {
		SBC_8(direct_indirect_indexed_long_8());
	}
	program_counter += 2;
}

//SBC addr, Y	F9	Absolute Indexed, Y	NV— - ZC	3
void fF9_SBC(){
	if (m_flag() == 0) {
		SBC_16(absolute_indexed_y_16());
	}
	else {
		SBC_8(absolute_indexed_y_8());
	}
	program_counter += 3;
}

//SBC addr, X	FD	Absolute Indexed, X	NV— - ZC	3
void fFD_SBC(){
	if (m_flag() == 0) {
		SBC_16(absolute_indexed_x_16());
	}
	else {
		SBC_8(absolute_indexed_x_8());
	}
	program_counter += 3;
}

//SBC long, X	FF	Absolute Long Indexed, X	NV— - ZC	4
void fFF_SBC(){
	if (m_flag() == 0) {
		SBC_16(absolute_indexed_long_16());
	}
	else {
		SBC_8(absolute_indexed_long_8());
	}
	program_counter += 4;
}
#pragma endregion

#pragma region p_register
//REP #const	C2	Immediate	NVMXDIZC	2
void fC2_REP() {
	uint8_t operand = immediate_8();
	p_register &= ~operand;
	program_counter += 2;
}

//SEC	38	Implied	—— - C	1
void f38_SEC(){
	p_register |= CARRY_FLAG;
	program_counter++;
}

//SED	F8	Implied	— - D—	1
void fF8_SED(){
	p_register |= DECIMAL_FLAG;
	program_counter++;
}

//SEI	78	Implied	—–I–	1
void f78_SEI(){
	p_register |= INTERRUPT_FLAG;
	program_counter++;
}

//SEP	E2	Immediate	NVMXDIZC	2
void fE2_SEP(){
	uint8_t operand = immediate_8();
	p_register |= operand;
	program_counter += 2;
}
#pragma endregion

#pragma region store

void STA_8(uint8_t* local_addr) {
	*local_addr = accumulator;
}

void STA_16(uint8_t* local_addr) {
	store2Byte_local(local_addr, accumulator);
}

//STA(_dp, _X)	81	DP Indexed Indirect, X		2
void f81_STA(){
	uint8_t *addr = access_address(direct_indexed_indirect());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA sr, S	83	Stack Relative		2
void f83_STA(){
	uint8_t *addr = access_address_from_bank(data_bank_register >> 16 ,stack_relative());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA dp	85	Direct Page		2
void f85_STA(){
	uint8_t *addr = access_address_from_bank(data_bank_register >> 16, direct());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA[_dp_]	87	DP Indirect Long		2
void f87_STA(){
	uint8_t *addr = access_address(direct_indirect_long());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA addr	8D	Absolute		3
void f8D_STA(){
	uint8_t *addr = access_address(absolute());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 3;
}

//STA long	8F	Absolute Long		4
void f8F_STA(){
	uint8_t *addr = access_address( absolute_long());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 4;
}

//STA(_dp_), Y	91	DP Indirect Indexed, Y		2
void f91_STA(){
	uint8_t *addr = access_address(direct_indirect_indexed());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA(_dp_)	92	DP Indirect		2
void f92_STA(){
	uint8_t *addr = access_address(direct_indirect_indexed());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA(_sr_, S), Y	93	SR Indirect Indexed, Y		2
void f93_STA(){
	uint8_t *addr = access_address(direct_indirect_indexed());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA dpX	95	DP Indexed, X		2
void f95_STA(){
	uint8_t *addr = access_address(direct_indexed_x());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA[_dp_], Y	97	DP Indirect Long Indexed, Y		2
void f97_STA(){
	uint8_t *addr = access_address(direct_indirect_indexed_long());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 2;
}

//STA addr, Y	99	Absolute Indexed, Y		3
void f99_STA(){
	uint8_t *addr = access_address_from_bank(data_bank_register >> 16, absolute_indexed_y());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 3;
}

//STA addr, X	9D	Absolute Indexed, X		3
void f9D_STA(){
	uint8_t *addr = access_address_from_bank(data_bank_register >> 16, absolute_indexed_x());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 3;
}

//STA long, X	9F	Absolute Long Indexed, X		4
void f9F_STA(){
	uint8_t *addr = access_address(absolute_indirect_long());
	if (m_flag() == 0)
		STA_16(addr);
	else
		STA_8(addr);

	program_counter += 4;
}

void STN_8(uint8_t* local_addr, uint8_t val) {
	*local_addr = val;
}

void STN_16(uint8_t* local_addr, uint16_t val) {
	store2Byte_local(local_addr, val);
}

//STX dp	86	Direct Page		2
void f86_STX(){
	uint8_t *addr = access_address_from_bank(data_bank_register, direct());
	if (m_flag() == 0)
		STN_16(addr, X);
	else
		STN_8(addr, X);

	program_counter += 2;
}

//STX addr	8E	Absolute		3
void f8E_STX(){
	uint8_t *addr = access_address(absolute());
	if (m_flag() == 0)
		STN_16(addr, X);
	else
		STN_8(addr, X);

	program_counter += 3;
}

//STX dp, Y	96	DP Indexed, Y		2
void f96_STX(){
	uint8_t *addr = access_address(direct_indexed_y());
	if (m_flag() == 0)
		STN_16(addr, X);
	else
		STN_8(addr, X);

	program_counter += 2;
}

//STY dp	84	Direct Page		2
void f84_STY(){
	uint8_t *addr = access_address_from_bank(data_bank_register, direct());
	if (m_flag() == 0)
		STN_16(addr, Y);
	else
		STN_8(addr, Y);

	program_counter += 2;
}

//STY addr	8C	Absolute		3
void f8C_STY(){
	uint8_t *addr = access_address(absolute());
	if (m_flag() == 0)
		STN_16(addr, Y);
	else
		STN_8(addr, Y);

	program_counter += 3;
}

//STY dp, X	94	DP Indexed, X		2
void f94_STY(){
	uint8_t *addr = access_address(direct_indexed_x());
	if (m_flag() == 0)
		STN_16(addr, Y);
	else
		STN_8(addr, Y);

	program_counter += 2;
}

//STZ dp	64	Direct Page		2
void f64_STZ(){
	uint8_t *addr = access_address_from_bank(data_bank_register, direct());
	if (m_flag() == 0)
		STN_16(addr, 0x0000);
	else
		STN_8(addr, 0x0000);

	program_counter += 2;
}

//STZ dp, X	74	DP Indexed, X		2
void f74_STZ(){
	uint8_t *addr = access_address(direct_indexed_x());
	if (m_flag() == 0)
		STN_16(addr, 0x0000);
	else
		STN_8(addr, 0x0000);

	program_counter += 2;
}

//STZ addr	9C	Absolute		3
void f9C_STZ(){
	uint8_t *addr = access_address(absolute());
	if (m_flag() == 0)
		STN_16(addr, 0x0000);
	else
		STN_8(addr, 0x0000);

	program_counter += 3;
}

//STZ addr, X	9E	Absolute Indexed, X		3
void f9E_STZ(){
	uint8_t *addr = access_address_from_bank(data_bank_register, absolute_indexed_x());
	if (m_flag() == 0)
		STN_16(addr, 0x0000);
	else
		STN_8(addr, 0x0000);

	program_counter += 3;
}

#pragma endregion

#pragma region test_reset

//TRB dp	14	Direct Page	——Z - 2
void f14_TRB(){
	uint8_t *local_addr = access_address_from_bank(data_bank_register, direct());
	if (m_flag() == 0) {
		uint16_t val = get2Byte(local_addr);
		val &= accumulator;
		store2Byte(local_addr, val);
		set_p_register_16(val, ZERO_FLAG);
	}
	else {
		*local_addr = accumulator & *local_addr;
		set_p_register_8(*local_addr, ZERO_FLAG);
	}

}

//TRB addr	1C	Absolute	——Z - 3
void f1C_TRB(){
	uint8_t *local_addr = access_address(absolute());
	if (m_flag() == 0) {
		uint16_t val = get2Byte(local_addr);
		val &= accumulator;
		store2Byte(local_addr, val);
		set_p_register_16(val, ZERO_FLAG);
	}
	else {
		*local_addr = accumulator & *local_addr;
		set_p_register_8(*local_addr, ZERO_FLAG);
	}
}

//TSB dp	4	Direct Page	——Z - 2
void f04_TSB(){
	uint8_t *local_addr = access_address_from_bank(data_bank_register, direct());
	if (m_flag() == 0) {
		uint16_t val = get2Byte(local_addr);
		val |= accumulator;
		store2Byte(local_addr, val);
		set_p_register_16(val, ZERO_FLAG);
	}
	else {
		*local_addr = accumulator | *local_addr;
		set_p_register_8(*local_addr, ZERO_FLAG);
	}
}

//TSB addr	0C	Absolute	——Z - 3
void f0C_TSB(){
	uint8_t *local_addr = access_address(absolute());
	if (m_flag() == 0) {
		uint16_t val = get2Byte(local_addr);
		val |= accumulator;
		store2Byte(local_addr, val);
		set_p_register_16(val, ZERO_FLAG);
	}
	else {
		*local_addr = accumulator | *local_addr;
		set_p_register_8(*local_addr, ZERO_FLAG);
	}
}
#pragma endregion

#pragma region transfers
//TAX	AA	Implied	N—–Z - 1
void fAA_TAX() {
	X = accumulator;
	if (x_flag() == 0)
		set_p_register_16(X, NEGATIVE_FLAG, ZERO_FLAG);
	else
		set_p_register_8(X, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TAY	A8	Implied	N—–Z - 1
void fA8_TAY() {
	Y = accumulator;
	if (x_flag() == 0)
		set_p_register_16(X, NEGATIVE_FLAG, ZERO_FLAG);
	else
		set_p_register_8(X, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TCD	5B	Implied	N—–Z - 1
void f5B_TCD() {
	direct_page = accumulator;
	set_p_register_16(direct_page, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TCS	1B	Implied		1
void f1B_TCS() {
	stack = accumulator;
	set_p_register_16(stack, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TDC	7B	Implied	N—–Z - 1
void f7B_TDC() {
	accumulator = direct_page;
	set_p_register_16(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TSC	3B	Implied	N—–Z - 1
void f3B_TSC(){
	accumulator = stack;
	set_p_register_16(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TSX	BA	Implied	N—–Z - 1
void fBA_TSX(){
	X = stack;
	set_p_register_16(X, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TXA	8A	Implied	N—–Z - 1
void f8A_TXA(){
	accumulator = X;
	set_p_register_16(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TXS	9A	Implied		1
void f9A_TXS(){
	stack = X;
	set_p_register_16(stack, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TXY	9B	Implied	N—–Z - 1
void f9B_TXY(){
	Y = X;
	set_p_register_16(Y, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TYA	98	Implied	N—–Z - 1
void f98_TYA(){
	accumulator = Y;
	set_p_register_16(accumulator, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}

//TYX	BB	Implied	N—–Z - 1
void fBB_TYX(){
	X = Y;
	set_p_register_16(X, NEGATIVE_FLAG, ZERO_FLAG);
	program_counter++;
}
#pragma endregion

//WAI	CB	Implied		1
void fCB_WAI(){}

//STP	DB	Implied		1
void fDB_STP() {}

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
	uint8_t carry_holder = p_register & CARRY_FLAG;
	p_register &= !CARRY_FLAG;
	p_register |= emulation_flag;
	emulation_flag = carry_holder;
	
	if (emulation_flag == 0x0) {
		inEmulationMode = 0;
	}
	else {
		inEmulationMode = 1;
	}
	program_counter++;
}


#pragma endregion