#include "cartridge.h"
#include <stdio.h>

//Load rom from file into dynamically allocated memory
//Returns 0 if success, -1 if rom size incorrect
int loadRom(const char* filepath) {
	FILE *romFile = fopen(filepath, "rb");
	//Get rom size
	fseek(romFile, 0, SEEK_END);
	long rom_size = ftell(romFile);
	fseek(romFile, 0, SEEK_SET);
	if (rom_size <= 0 || rom_size > 12580000)
		return 1;

	emulated_cartidge.rom = malloc(rom_size);
	fread(emulated_cartidge.rom, rom_size, 1, romFile);	//Read rom into memoryk
	emulated_cartidge.romLoaded = 1;
	emulated_cartidge.size = rom_size;

	int	hi_score, lo_score;

	hi_score = ScoreHiROM(1);
	lo_score = ScoreLoROM(1);


	emulated_cartidge.romType = HiROM;
	if(lo_score > hi_score)
		emulated_cartidge.romType = LoROM;
	return 0;
}

//Give an estimate on probability of a header existing at offset atPoint
unsigned int checkRomType(long atPoint) {
	//check if sizes are completely off
	if (emulated_cartidge.size < atPoint + 64)
		return -1;


}

void deleteRom() {
	if (emulated_cartidge.romLoaded == 1)
		free(emulated_cartidge.rom);
}

int ScoreHiROM(_Bool skip_header) {
	long	*buf = emulated_cartidge.rom + 0xff00 + 0 + (skip_header ? 0x200 : 0);
	int		score = 0;

	if (buf[0xd5] & 0x1)
		score += 2;

	// Mode23 is SA-1
	if (buf[0xd5] == 0x23)
		score -= 2;

	if (buf[0xd4] == 0x20)
		score += 2;

	if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff) {
		score += 2;
		if (0 != (buf[0xde] + (buf[0xdf] << 8)))
			score++;
	}

	if (buf[0xda] == 0x33)
		score += 2;

	if ((buf[0xd5] & 0xf) < 4)
		score += 2;

	if (!(buf[0xfd] & 0x80))
		score -= 6;

	if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
		score -= 2; // reduced after looking at a scan by Cowering

	if (emulated_cartidge.size > 1024 * 1024 * 3)
		score += 4;

	if ((1 << (buf[0xd7] - 7)) > 48)
		score -= 1;

	return (score);
}

int ScoreLoROM(_Bool skip_header) {
	long	*buf = emulated_cartidge.rom + 0x7f00 + 0 + (skip_header ? 0x200 : 0);
	int		score = 0;

	if (!(buf[0xd5] & 0x1))
		score += 3;

	// Mode23 is SA-1
	if (buf[0xd5] == 0x23)
		score += 2;

	if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff) {
		score += 2;
		if (0 != (buf[0xde] + (buf[0xdf] << 8)))
			score++;
	}

	if (buf[0xda] == 0x33)
		score += 2;

	if ((buf[0xd5] & 0xf) < 4)
		score += 2;

	if (!(buf[0xfd] & 0x80))
		score -= 6;

	if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
		score -= 2; // reduced per Cowering suggestion

	if (emulated_cartidge.size <= 1024 * 1024 * 16)
		score += 2;

	if ((1 << (buf[0xd7] - 7)) > 48)
		score -= 1;


	return (score);
}