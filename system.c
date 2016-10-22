#include "system.h"
#include "cartridge.h"

char *access_address(char bank, short addr) {

}

char *access_address(unsigned int addr) {
	char bank = addr >> 16;

}

int load_rom(const char* rom_path) {
	return loadRom(rom_path);
}
