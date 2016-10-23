#include "system.h"
#include "cartridge.h"
#include "ram.h"
#include "cpu.h"

#pragma region private_headers
char *access_address_from_bank_loRom(char bank, unsigned short addr);
char *access_address_from_bank_hiRom(char bank, unsigned short addr);
void cycle();
_Bool execute;
unsigned int cycle_counter;
char system_memory[131072];
char hardware_registers[16383];
#pragma endregion

char *access_address(unsigned int addr) {
	char bank = addr >> 16;
	short offset = addr & 0xFFFF;
	return access_address_from_bank(bank, offset);
}

int load_rom(const char* rom_path) {
	return loadRom(rom_path);
}

char *getRomData() {
	return emulated_cartidge.rom;
}

int startup() {
	if(emulated_cartidge.romType == LoROM)
		access_address_from_bank = access_address_from_bank_loRom;
	else if (emulated_cartidge.romType == HiROM)
		access_address_from_bank = access_address_from_bank_hiRom;
	initialise_cpu();
}

void begin_execution() {
	if (emulated_cartidge.romType == LoROM)
		program_counter = 0xff70;
	else if (emulated_cartidge.romType == HiROM)
		program_counter = 0xff70;// 0x101FA;
	execute = 1;
	cycle();
}

void cycle() {
	while (execute) {
		if (cycle_counter == 1) {

		}
		else {
			execute_next_instruction();
		}
		cycle_counter++;
	}
}

char *access_address_from_bank_loRom(char bank, unsigned short offset) {
	if (bank >= 0x00 && bank <= 0x3F) {
		//Shadow ram
		if (offset >= 0x0000 && offset <= 0x1FFF) {
			int shadow_addr = 0;
			shadow_addr = shadow_addr | offset;
			return &system_memory[shadow_addr];
		}
		//Hardware addresses
		if (offset >= 0x2000 && offset <= 0x5FFF) {
			return &hardware_registers[offset];
		}
		//Expansion ram
		if (offset >= 0x6000 && offset <= 0x7FFF) {
			return 0;
		}
		//Rom mapping
		if (offset >= 0x8000 && offset <= 0xFFFF) {
			int romIndex = (bank * 0x7FFF) + offset;
			return &emulated_cartidge.rom[romIndex];
		}
	}
	//Further rom mapping
	if (bank >= 0x40 && bank <= 0x7C) {
		int romIndex = (0x3F * 0x7FFF) + ((bank * 0xFFFF) + offset);
		return &emulated_cartidge.rom[romIndex];
	}
	//Sram
	if (bank == 0x7D) {
		return &emulated_cartidge.sram[offset];
	}
	//System ram
	if (bank >= 0x7E && bank <= 0x7F) {
		int ram_addr = (bank - 0x7E) * 0xFFFF + offset;
		return &system_memory[ram_addr];
	}

	//Fast rom
	if (bank >= 0x80 && bank <= 0xFF) {
		if (offset & 0xFF00)
			;// TODO reset vectors
		else
			return access_address_from_bank(0x80 - bank, offset);
	}

	return NULL;
}

char *access_address_from_bank_hiRom(char bank, unsigned short offset) {
	if (bank >= 0x00 && bank <= 0x3F) {
		//Shadow ram
		if (offset >= 0x0000 && offset <= 0x1FFF) {
			int shadow_addr = 0;
			shadow_addr = shadow_addr | offset;
			return &system_memory[shadow_addr];
		}
		//Hardware addresses
		if (offset >= 0x2000 && offset <= 0x5FFF) {
			return &hardware_registers[offset];
		}
		//sram
		if (offset >= 0x6000 && offset <= 0x7FFF) {
			int sramIndex = (bank * 0x1FFF) + (offset-0x6000);
			return &emulated_cartidge.rom[sramIndex];
		}
		//Rom mapping
		if (offset >= 0x8000 && offset <= 0xFFFF) {
			int romIndex = (bank * 0x7FFF) + offset;
			return &emulated_cartidge.rom[romIndex];
		}
	}
	//Further rom mapping
	if (bank >= 0x40 && bank <= 0x7D) {
		int romIndex =  (((bank- 0x40) * 0xFFFF) + offset);
		return &emulated_cartidge.rom[romIndex];
	}
	//System ram
	if (bank >= 0x7E && bank <= 0x7F) {
		int ram_addr = (bank - 0x7E) * 0xFFFF + offset;
		return &system_memory[ram_addr];
	}

	//Fast rom
	if (bank >= 0x80 && bank <= 0xFD) {
		return access_address_from_bank(bank - 0x80, offset);
	}
	//Last bt of rom
	if (bank >= 0xFE && bank <= 0xFF) {
		if (offset & 0xFF00)
			;// TODO reset vectors
		else {
			int romIndex = (0x3D * 0xFFFF) + (((bank-0xFE) * 0xFFFF) + offset);
			return &emulated_cartidge.rom[romIndex];
		}
	}
	return NULL;
}