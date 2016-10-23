#include "main.h"

int main() {
	read_rom_from_file("smk.sfc");
	int startupSuccess = startup();
	begin_execution();
	return 0;
}

int read_rom_from_file(const char* romLoc) {
	return load_rom(romLoc);
}

