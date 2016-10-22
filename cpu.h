#pragma once

void initialise_cpu();
void execute_next_instruction();

void populate_instructions();

char registers[30 * 2];




#pragma region Registers

#pragma region Internal_Registers
short accumulator;
unsigned int program_counter;
unsigned int stack_pointer;
unsigned int direct_page;
#pragma endregion

#pragma endregion

#pragma region INSTRUCTIONS

void(*instructions[16 * 16])();

//ORA (_dp, _X) 0x01
void ORA_01();

//ORA st,S 0x03
void ORA_03();

//ORA dp 0x05
void ORA_05();

#pragma endregion