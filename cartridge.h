#pragma once
#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdlib.h>
#include <stdint.h>

#define LO_ROM			0x20
#define HI_ROM			0x21
#define LO_ROM_FASTROM	0x30
#define HI_ROM_FASTROM	0x31
#define EX_LOROM			0x32
#define EX_HIROM			0x35

struct Emulated_Cartridge {
	uint8_t* rom;
	uint8_t* sram;
	_Bool rom_loaded;
	long size;
	short rom_type;
} emulated_cartidge;

int LoadRom(const char* filepath);
void DeleteRom();
int	ScoreHiROM(_Bool skip_header);
int	ScoreLoROM(_Bool skip_header);


#endif