
#include "cpu.h"
#include "ram.h"


void initialise_cpu() {
	stack_pointer = ram + 0x7FFFFF;
	ram[stack_pointer] = (short)0x1FFF;
	direct_page = (short)0000;


}

void execute_next_instruction() {
	char current_instruction = ram[program_counter];
	(*instructions[current_instruction])();

}


void populate_instructions() {
	instructions[0x01] = ORA_01;
	instructions[0x03] = ORA_03;
	instructions[0x05] = ORA_05;
}

#pragma region INSTRUCTIONS

//ORA (_dp, _X) 0x01 size 2 bytes
void ORA_01() {
	short *operand = &ram[program_counter + 1];
	short *dpVal = &ram[direct_page];
	short *value = &ram[*dpVal + *operand];
	accumulator = accumulator | *value;
}

//ORA st,S 0x03
void ORA_03() {
	accumulator = accumulator | *((short*)&ram[stack_pointer]);
}

//ORA dp 0x05
void ORA_05() {
	accumulator = accumulator | *((short*)&ram[direct_page]);
}

//ORA [_dp_] 0x07
void ORA_07() {
	short *operand = &ram[program_counter + 1];
	short *dpVal = &ram[direct_page];
	int *value = &ram[*dpVal];
	accumulator = accumulator | *value;
}

#pragma endregion