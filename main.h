#pragma once
#include <cstdlib>
#include "cpu.h"


int main();

void read_bios(const char* bios_location);
void read_rom_file(const char* rom_loaction);

void populate_instructions;

void map_memory();


void *rom;
