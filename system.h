#pragma once

char *access_address(char bank, short addr);

char *access_address(unsigned int addr);

int load_rom(const char* rom_path);