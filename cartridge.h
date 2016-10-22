#pragma once
#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdlib.h>
#include <stdio.h>

#define LoROM			0x20
#define HiROM			0x21
#define LoROM_FastROM	0x30
#define HiROM_FastROM	0x31
#define ExLoROM			0x32
#define ExHiROM			0x35

struct Emulated_Cartridge {
	char* rom;
	_Bool romLoaded;
	long size;
	short romType;
} emulated_cartidge;

int loadRom(const char* filepath);
void deleteRom();
int	ScoreHiROM(_Bool skipHeader);
int	ScoreLoROM(_Bool skipHeader);


#endif