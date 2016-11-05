#pragma once
#include <stdint.h>

void initialise_cpu();
void execute_next_instruction();

void populate_instructions();

char registers[30 * 2];

#pragma region Registers
uint16_t p_register;

#pragma region Internal_Registers
uint16_t accumulator;
uint32_t program_counter;
uint8_t *stack_pointer;
uint32_t stack;
uint16_t data_bank_register;
uint16_t program_bank_register;
uint16_t direct_page;
uint16_t X, Y;
uint8_t emulation_flag; //emulation flag, lowest bit only

#define CARRY_FLAG  0x1
#define ZERO_FLAG 0x2
#define INTERRUPT_FLAG 0x04
#define DECIMAL_FLAG 0x08
#define OVERFLOW_FLAG 0x40
#define NEGATIVE_FLAG 0x80
#define M_FLAG 0x20
#define X_FLAG 0x10

#pragma endregion

#pragma endregion


#pragma region INSTRUCTIONS

void(*instructions[16 * 16])();

//ADC(_dp_, X)	61	DP Indexed Indirect, X	NV— - ZC	2
void f61_ADC();

//ADC sr, S	63	Stack Relative	NV— - ZC	2
void f63_ADC();

//ADC dp	65	Direct Page	NV— - ZC	2
void f65_ADC();

//ADC[_dp_]	67	DP Indirect Long	NV— - ZC	2
void f67_ADC();

//ADC #const	69	Immediate	NV— - ZC	23
void f69_ADC();

//ADC addr	6D	Absolute	NV— - ZC	3
void f6D_ADC();

//ADC long	6F	Absolute Long	NV— - ZC	4
void f6F_ADC();

//ADC(dp), Y	71	DP Indirect Indexed, Y	NV— - ZC	2
void f71_ADC();

//ADC(_dp_)	72	DP Indirect	NV— - ZC	2
void f72_ADC();

//ADC(_sr_, S), Y	73	SR Indirect Indexed, Y	NV— - ZC	2
void f73_ADC();

//ADC dp, X	75	DP Indexed, X	NV— - ZC	2
void f75_ADC();

//ADC[_dp_], Y	77	DP Indirect Long Indexed, Y	NV— - ZC	2
void f77_ADC();

//ADC addr, Y	79	Absolute Indexed, Y	NV— - ZC	3
void f79_ADC();

//ADC addr, X	7D	Absolute Indexed, X	NV— - ZC	3
void f7D_ADC();

//ADC long, X	7F	Absolute Long Indexed, X	NV— - ZC	4
void f7F_ADC();

//AND(_dp, _X)	21	DP Indexed Indirect, X	N—–Z - 2
void f21_AND();

//AND sr, S	23	Stack Relative	N—–Z - 2
void f23_AND();

//AND dp	25	Direct Page	N—–Z - 2
void f25_AND();

//AND[_dp_]	27	DP Indirect Long	N—–Z - 2
void f27_AND();

//AND #const	29	Immediate	N—–Z - 2
void f29_AND();

//AND addr	2D	Absolute	N—–Z - 3
void f2D_AND();

//AND long	2F	Absolute Long	N—–Z - 4
void f2F_AND();

//AND(_dp_), Y	31	DP Indirect Indexed, Y	N—–Z - 2
void f31_AND();

//AND(_dp_)	32	DP Indirect	N—–Z - 2
void f32_AND();

//AND(_sr_, S), Y	33	SR Indirect Indexed, Y	N—–Z - 2
void f33_AND();

//AND dp, X	35	DP Indexed, X	N—–Z - 2
void f35_AND();

//AND[_dp_], Y	37	DP Indirect Long Indexed, Y	N—–Z - 2
void f37_AND();

//AND addr, Y	39	Absolute Indexed, Y	N—–Z - 3
void f39_AND();

//AND addr, X	3D	Absolute Indexed, X	N—–Z - 3
void f3D_AND();

//AND long, X	3F	Absolute Long Indexed, X	N—–Z - 4
void f3F_AND();

//ASL dp	6	Direct Page	N—–ZC	2
void f06_ASL();

//ASL A	0A	Accumulator	N—–ZC	1
void f0A_ASL();

//ASL addr	0E	Absolute	N—–ZC	3
void f0E_ASL();

//ASL dp, X	16	DP Indexed, X	N—–ZC	2
void f16_ASL();

//ASL addr, X	1E	Absolute Indexed, X	N—–ZC	3
void f1E_ASL();

//BCC nearlabel	90	Program Counter Relative		2
void f90_BCC();

//BCS nearlabel	B0	Program Counter Relative		2
void fB0_BCs();

//BEQ nearlabel	F0	Program Counter Relative		2
void fF0_BEQ();

//BIT dp	24	Direct Page	NV— - Z - 2
void f24_BIT();

//BIT addr	2C	Absolute	NV— - Z - 3
void f2C_BIT();

//BIT dp, X	34	DP Indexed, X	NV— - Z - 2
void f34_BIT();

//BIT addr, X	3C	Absolute Indexed, X	NV— - Z - 3
void f3C_BIT();

//BIT #const	89	Immediate	——Z - 2
void f89_BIT();

//BMI nearlabel	30	Program Counter Relative		2
void f30_BMI();

//BNE nearlabel	D0	Program Counter Relative		2
void fD0_BNE();

//BPL nearlabel	10	Program Counter Relative		2
void f10_BPL();

//BRA nearlabel	80	Program Counter Relative		2
void f80_BRA();

//BRK	0	Stack / Interrupt	— - DI–	28
void f00_BRK();

//BRL label	82	Program Counter Relative Long		3
void f82_BRL();

//BVC nearlabel	50	Program Counter Relative		2
void f50_BVC();

//BVS nearlabel	70	Program Counter Relative		2
void f70_BVS();

//CLC	18	Implied	—— - C	1
void f18_CLC();

//CLD	D8	Implied	— - D—	1
void fD8_CLD();

//CLI	58	Implied	—–I–	1
void f58_CLI();

//CLV	B8	Implied	#NAME ? 1
void fB8_CLV();

//CMP(_dp, _X)	C1	DP Indexed Indirect, X	N—–ZC	2
void fC1_CMP();

//CMP sr, S	C3	Stack Relative	N—–ZC	2
void fC3_CMP();

//CMP dp	C5	Direct Page	N—–ZC	2
void fC5_CMP();

//CMP[_dp_]	C7	DP Indirect Long	N—–ZC	2
void fC7_CMP();

//CMP #const	C9	Immediate	N—–ZC	2
void fC9_CMP();

//CMP addr	CD	Absolute	N—–ZC	3
void fCD_CMP();

//CMP long	CF	Absolute Long	N—–ZC	4
void fCF_CMP();

//CMP(_dp_), Y	D1	DP Indirect Indexed, Y	N—–ZC	2
void fD1_CMP();

//CMP(_dp_)	D2	DP Indirect	N—–ZC	2
void fD2_CMP();

//CMP(_sr_, S), Y	D3	SR Indirect Indexed, Y	N—–ZC	2
void fD3_CMP();

//CMP dp, X	D5	DP Indexed, X	N—–ZC	2
void fD5_CMP();

//CMP[_dp_], Y	D7	DP Indirect Long Indexed, Y	N—–ZC	2
void fD7_CMP();

//CMP addr, Y	D9	Absolute Indexed, Y	N—–ZC	3
void fD9_CMP();

//CMP addr, X	DD	Absolute Indexed, X	N—–ZC	3
void fDD_CMP();

//CMP long, X	DF	Absolute Long Indexed, X	N—–ZC	4
void fDF_CMP();

//COP const	2	Stack / Interrupt	— - DI–	2
void f02_COP();

//CPX #const	E0	Immediate	N—–ZC	210
void fE0_CPX();

//CPX dp	E4	Direct Page	N—–ZC	2
void fE4_CPX();

//CPX addr	EC	Absolute	N—–ZC	3
void fEC_CPX();

//CPY #const	C0	Immediate	N—–ZC	2
void fC0_CPY();

//CPY dp	C4	Direct Page	N—–ZC	2
void fC4_CPY();

//CPY addr	CC	Absolute	N—–ZC	3
void fCC_CPY();

//DEC A	3A	Accumulator	N—–Z - 1
void f3A_DEC();

//DEC dp	C6	Direct Page	N—–Z - 2
void fC6_DEC();

//DEC addr	CE	Absolute	N—–Z - 3
void fCE_DEC();

//DEC dp, X	D6	DP Indexed, X	N—–Z - 2
void fD6_DEC();

//DEC addr, X	DE	Absolute Indexed, X	N—–Z - 3
void fDE_DEC();

//DEX	CA	Implied	N—–Z - 1
void fCA_DEX();

//DEY	88	Implied	N—–Z - 1
void f88_DEY();

//EOR(_dp, _X)	41	DP Indexed Indirect, X	N—–Z - 2
void f41_EOR();

//EOR sr, S	43	Stack Relative	N—–Z - 2
void f43_EOR();

//EOR dp	45	Direct Page	N—–Z - 2
void f45_EOR();

//EOR[_dp_]	47	DP Indirect Long	N—–Z - 2
void f47_EOR();

//EOR #const	49	Immediate	N—–Z - 2
void f49_EOR();

//EOR addr	4D	Absolute	N—–Z - 3
void f4D_EOR();

//EOR long	4F	Absolute Long	N—–Z - 4
void f4F_EOR();

//EOR(_dp_), Y	51	DP Indirect Indexed, Y	N—–Z - 2
void f51_EOR();

//EOR(_dp_)	52	DP Indirect	N—–Z - 2
void f52_EOR();

//EOR(_sr_, S), Y	53	SR Indirect Indexed, Y	N—–Z - 2
void f53_EOR();

//EOR dp, X	55	DP Indexed, X	N—–Z - 2
void f55_EOR();

//EOR[_dp_], Y	57	DP Indirect Long Indexed, Y	N—–Z - 2
void f57_EOR();

//EOR addr, Y	59	Absolute Indexed, Y	N—–Z - 3
void f59_EOR();

//EOR addr, X	5D	Absolute Indexed, X	N—–Z - 3
void f5D_EOR();

//EOR long, X	5F	Absolute Long Indexed, X	N—–Z - 4
void f5F_EOR();

//INC A	1A	Accumulator	N—–Z - 1
void f1A_INC();

//INC dp	E6	Direct Page	N—–Z - 2
void fE6_INC();

//INC addr	EE	Absolute	N—–Z - 3
void fEE_INC();

//INC dp, X	F6	DP Indexed, X	N—–Z - 2
void fF6_INC();

//INC addr, X	FE	Absolute Indexed, X	N—–Z - 3
void fFE_INC();

//INX	E8	Implied	N—–Z - 1
void fE8_INX();

//INY	C8	Implied	N—–Z - 1
void fC8_INY();

//JMP addr	4C	Absolute		3
void f4C_JMP();

//JMP long	5C	Absolute Long		4
void f5C_JMP();

//JMP(_addr_)	6C	Absolute Indirect		3
void f6C_JMP();

//JMP(_addr, X_)	7C	Absolute Indexed Indirect		3
void f7C_JMP();

//JMP[addr]	DC	Absolute Indirect Long		3
void fDC_JMP();

//JSR addr	20	Absolute		3
void f20_JSR();

//JSR long	22	Absolute Long		4
void f22_JSR();

//JSR(addr, X))	FC	Absolute Indexed Indirect		3
void fFC_JSR();

//LDA(_dp, _X)	A1	DP Indexed Indirect, X	N—–Z - 2
void fA1_LDA();

//LDA sr, S	A3	Stack Relative	N—–Z - 2
void fA3_LDA();

//LDA dp	A5	Direct Page	N—–Z - 2
void fA5_LDA();

//LDA[_dp_]	A7	DP Indirect Long	N—–Z - 2
void fA7_LDA();

//LDA #const	A9	Immediate	N—–Z - 2
void fA9_LDA();

//LDA addr	AD	Absolute	N—–Z - 3
void fAD_LDA();

//LDA long	AF	Absolute Long	N—–Z - 4
void fAF_LDA();

//LDA(_dp_), Y	B1	DP Indirect Indexed, Y	N—–Z - 2
void fB1_LDA();

//LDA(_dp_)	B2	DP Indirect	N—–Z - 2
void fB2_LDA();

//LDA(_sr_, S), Y	B3	SR Indirect Indexed, Y	N—–Z - 2
void fB3_LDA();

//LDA dp, X	B5	DP Indexed, X	N—–Z - 2
void fB5_LDA();

//LDA[_dp_], Y	B7	DP Indirect Long Indexed, Y	N—–Z - 2
void fB7_LDA();

//LDA addr, Y	B9	Absolute Indexed, Y	N—–Z - 3
void fB9_LDA();

//LDA addr, X	BD	Absolute Indexed, X	N—–Z - 3
void fBD_LDA();

//LDA long, X	BF	Absolute Long Indexed, X	N—–Z - 4
void fBF_LDA();

//LDX #const	A2	Immediate	N—–Z - 212
void fA2_LDX();

//LDX dp	A6	Direct Page	N—–Z - 2
void fA6_LDX();

//LDX addr	AE	Absolute	N—–Z - 3
void fAE_LDX();

//LDX dp, Y	B6	DP Indexed, Y	N—–Z - 2
void fB6_LDX();

//LDX addr, Y	BE	Absolute Indexed, Y	N—–Z - 3
void fBE_LDX();

//LDY #const	A0	Immediate	N—–Z - 2
void fA0_LDY();

//LDY dp	A4	Direct Page	N—–Z - 2
void fA4_LDY();

//LDY addr	AC	Absolute	N—–Z - 3
void fAC_LDY();

//LDY dp, X	B4	DP Indexed, X	N—–Z - 2
void fB4_LDY();

//LDY addr, X	BC	Absolute Indexed, X	N—–Z - 3
void fBC_LDY();

//LSR dp	46	Direct Page	N—–ZC	2
void f46_LSR();

//LSR A	4A	Accumulator	N—–ZC	1
void f4A_LSR();

//LSR addr	4E	Absolute	N—–ZC	3
void f4E_LSR();

//LSR dp, X	56	DP Indexed, X	N—–ZC	2
void f56_LSR();

//LSR addr, X	5E	Absolute Indexed, X	N—–ZC	3
void f5E_LSR();

//MVN srcbk, destbk	54	Block Move		3
void f54_MVN();

//MVP srcbk, destbk	44	Block Move		3
void f44_MVP();

//NOP	EA	Implied		1
void fEA_NOP();

//ORA(_dp, _X)	1	DP Indexed Indirect, X	N—–Z - 2
void f01_ORA();

//ORA sr, S	3	Stack Relative	N—–Z - 2
void f03_ORA();

//ORA dp	5	Direct Page	N—–Z - 2
void f05_ORA();

//ORA[_dp_]	7	DP Indirect Long	N—–Z - 2
void f07_ORA();

//ORA #const	9	Immediate	N—–Z - 2
void f09_ORA();

//ORA addr	0D	Absolute	N—–Z - 3
void f0D_ORA();

//ORA long	0F	Absolute Long	N—–Z - 4
void f0F_ORA();

//ORA(_dp_), Y	11	DP Indirect Indexed, Y	N—–Z - 2
void f11_ORA();

//ORA(_dp_)	12	DP Indirect	N—–Z - 2
void f12_ORA();

//ORA(_sr_, S), Y	13	SR Indirect Indexed, Y	N—–Z - 2
void f13_ORA();

//ORA dp, X	15	DP Indexed, X	N—–Z - 2
void f15_ORA();

//ORA[_dp_], Y	17	DP Indirect Long Indexed, Y	N—–Z - 2
void f17_ORA();

//ORA addr, Y	19	Absolute Indexed, Y	N—–Z - 3
void f19_ORA();

//ORA addr, X	1D	Absolute Indexed, X	N—–Z - 3
void f1D_ORA();

//ORA long, X	1F	Absolute Long Indexed, X	N—–Z - 4
void f1F_ORA();

//PEA addr	F4	Stack(Absolute)		3
void fF4_PEA();

//PEI(dp)	D4	Stack(DP Indirect)		2
void fD4_PEI();

//PER label	62	Stack(PC Relative Long)		3
void f62_PER();

//PHA	48	Stack(Push)		1
void f48_PHA();

//PHB	8B	Stack(Push)		1
void f8B_PHB();

//PHD	0B	Stack(Push)		1
void f0B_PHD();

//PHK	4B	Stack(Push)		1
void f4B_PHK();

//PHP	8	Stack(Push)		1
void f08_PHP();

//PHX	DA	Stack(Push)		1
void fDA_PHX();

//PHY	5A	Stack(Push)		1
void f5A_PHY();

//PLA	68	Stack(Pull)	N—–Z - 1
void f68_PLA();

//PLB	AB	Stack(Pull)	N—–Z - 1
void fAB_PLB();

//PLD	2B	Stack(Pull)	N—–Z - 1
void f2B_PLD();

//PLP	28	Stack(Pull)	N—–Z - 1
void f28_PLP();

//PLX	FA	Stack(Pull)	N—–Z - 1
void fFA_PLX();

//PLY	7A	Stack(Pull)	N—–Z - 1
void f7A_PLY();

//REP #const	C2	Immediate	NVMXDIZC	2
void fC2_REP();

//ROL dp	26	Direct Page	N—–ZC	2
void f26_ROL();

//ROL A	2A	Accumulator	N—–ZC	1
void f2A_ROL();

//ROL addr	2E	Absolute	N—–ZC	3
void f2E_ROL();

//ROL dp, X	36	DP Indexed, X	N—–ZC	2
void f36_ROL();

//ROL addr, X	3E	Absolute Indexed, X	N—–ZC	3
void f3E_ROL();

//ROR dp	66	Direct Page	N—–ZC	2
void f66_ROR();

//ROR A	6A	Accumulator	N—–ZC	1
void f6A_ROR();

//ROR addr	6E	Absolute	N—–ZC	3
void f6E_ROR();

//ROR dp, X	76	DP Indexed, X	N—–ZC	2
void f76_ROR();

//ROR addr, X	7E	Absolute Indexed, X	N—–ZC	3
void f7E_ROR();

//RTI	40	Stack(RTI)	NVMXDIZC	1
void f40_RTI();

//RTL	6B	Stack(RTL)		1
void f6B_RTL();

//RTS	60	Stack(RTS)		1
void f60_RTS();

//SBC(_dp, _X)	E1	DP Indexed Indirect, X	NV— - ZC	2
void fE1_SBC();

//SBC sr, S	E3	Stack Relative	NV— - ZC	2
void fE3_SBC();

//SBC dp	E5	Direct Page	NV— - ZC	2
void fE5_SBC();

//SBC[_dp_]	E7	DP Indirect Long	NV— - ZC	2
void fE7_SBC();

//SBC #const	E9	Immediate	NV— - ZC	2
void fE9_SBC();

//SBC addr	ED	Absolute	NV— - ZC	3
void fED_SBC();

//SBC long	EF	Absolute Long	NV— - ZC	4
void fEF_SBC();

//SBC(_dp_), Y	F1	DP Indirect Indexed, Y	NV— - ZC	2
void fF1_SBC();

//SBC(_dp_)	F2	DP Indirect	NV— - ZC	2
void fF2_SBC();

//SBC(_sr_, S), Y	F3	SR Indirect Indexed, Y	NV— - ZC	2
void fF3_SBC();

//SBC dp, X	F5	DP Indexed, X	NV— - ZC	2
void fF5_SBC();

//SBC[_dp_], Y	F7	DP Indirect Long Indexed, Y	NV— - ZC	2
void fF7_SBC();

//SBC addr, Y	F9	Absolute Indexed, Y	NV— - ZC	3
void fF9_SBC();

//SBC addr, X	FD	Absolute Indexed, X	NV— - ZC	3
void fFD_SBC();

//SBC long, X	FF	Absolute Long Indexed, X	NV— - ZC	4
void fFF_SBC();

//SEC	38	Implied	—— - C	1
void f38_SEC();

//SED	F8	Implied	— - D—	1
void fF8_SED();

//SEI	78	Implied	—–I–	1
void f78_SEI();

//SEP	E2	Immediate	NVMXDIZC	2
void fE2_SEP();

//STA(_dp, _X)	81	DP Indexed Indirect, X		2
void f81_STA();

//STA sr, S	83	Stack Relative		2
void f83_STA();

//STA dp	85	Direct Page		2
void f85_STA();

//STA[_dp_]	87	DP Indirect Long		2
void f87_STA();

//STA addr	8D	Absolute		3
void f8D_STA();

//STA long	8F	Absolute Long		4
void f8F_STA();

//STA(_dp_), Y	91	DP Indirect Indexed, Y		2
void f91_STA();

//STA(_dp_)	92	DP Indirect		2
void f92_STA();

//STA(_sr_, S), Y	93	SR Indirect Indexed, Y		2
void f93_STA();

//STA dpX	95	DP Indexed, X		2
void f95_STA();

//STA[_dp_], Y	97	DP Indirect Long Indexed, Y		2
void f97_STA();

//STA addr, Y	99	Absolute Indexed, Y		3
void f99_STA();

//STA addr, X	9D	Absolute Indexed, X		3
void f9D_STA();

//STA long, X	9F	Absolute Long Indexed, X		4
void f9F_STA();

//STP	DB	Implied		1
void fDB_STP();

//STX dp	86	Direct Page		2
void f86_STX();

//STX addr	8E	Absolute		3
void f8E_STX();

//STX dp, Y	96	DP Indexed, Y		2
void f96_STX();

//STY dp	84	Direct Page		2
void f84_STY();

//STY addr	8C	Absolute		3
void f8C_STY();

//STY dp, X	94	DP Indexed, X		2
void f94_STY();

//STZ dp	64	Direct Page		2
void f64_STZ();

//STZ dp, X	74	DP Indexed, X		2
void f74_STZ();

//STZ addr	9C	Absolute		3
void f9C_STZ();

//STZ addr, X	9E	Absolute Indexed, X		3
void f9E_STZ();

//TAX	AA	Implied	N—–Z - 1
void fAA_TAX();

//TAY	A8	Implied	N—–Z - 1
void fA8_TAY();

//TCD	5B	Implied	N—–Z - 1
void f5B_TCD();

//TCS	1B	Implied		1
void f1B_TCS();

//TDC	7B	Implied	N—–Z - 1
void f7B_TDC();

//TRB dp	14	Direct Page	——Z - 2
void f14_TRB();

//TRB addr	1C	Absolute	——Z - 3
void f1C_TRB();

//TSB dp	4	Direct Page	——Z - 2
void f04_TSB();

//TSB addr	0C	Absolute	——Z - 3
void f0C_TSB();

//TSC	3B	Implied	N—–Z - 1
void f3B_TSC();

//TSX	BA	Implied	N—–Z - 1
void fBA_TSX();

//TXA	8A	Implied	N—–Z - 1
void f8A_TXA();

//TXS	9A	Implied		1
void f9A_TXS();

//TXY	9B	Implied	N—–Z - 1
void f9B_TXY();

//TYA	98	Implied	N—–Z - 1
void f98_TYA();

//TYX	BB	Implied	N—–Z - 1
void fBB_TYX();

//WAI	CB	Implied		1
void fCB_WAI();

//WDM	42			2
void f42_WDM();

//XBA	EB	Implied	N—–Z - 1
void fEB_XBA();

//XCE	FB	Implied	–MX—CE	1
void fFB_XCE();

#pragma endregion