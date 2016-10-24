#include "system.h"
#include "cartridge.h"
#include "ram.h"
#include "cpu.h"

#pragma region private_headers
uint8_t *access_address_from_bank_loRom(uint8_t bank, uint16_t addr);
uint8_t *access_address_from_bank_hiRom(uint8_t bank, uint16_t addr);
void cycle();
_Bool execute;
unsigned int cycle_counter;
uint8_t system_memory[131072];
uint8_t hardware_registers[16383];
#pragma endregion

uint8_t *access_address(unsigned int addr) {
	uint8_t bank = addr >> 16;
	uint16_t offset = addr & 0xFFFF;
	return access_address_from_bank(bank, offset);
}

int load_rom(const char* rom_path) {
	return loadRom(rom_path);
}

uint8_t *getRomData() {
	return emulated_cartidge.rom;
}

uint8_t *accessSystemRam() {
	return system_memory;
}


int startup() {
	if(emulated_cartidge.romType == LoROM)
		access_address_from_bank = access_address_from_bank_hiRom;
	else if (emulated_cartidge.romType == HiROM)
		access_address_from_bank = access_address_from_bank_hiRom;
	initialise_cpu();
}

void begin_execution() {
	if (emulated_cartidge.romType == LoROM)
		program_counter = 0x01ff70;
	else if (emulated_cartidge.romType == HiROM)
		program_counter =  0x01ff70;
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

uint8_t *access_address_from_bank_loRom(uint8_t bank, uint16_t offset) {
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
		if (offset > 0xFF00)
			;// TODO reset vectors
		else
			return access_address_from_bank(0x80 - bank, offset);
	}

	return NULL;
}

uint8_t *access_address_from_bank_hiRom(uint8_t bank, uint16_t offset) {
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
			int romIndex = (bank * 0x7FFF) + offset - 0x7FFF;
			//0x01ff70;
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
		uint8_t newBank = bank - 0x80;
		if (newBank >= 0 && newBank <= 0x3F) {
			int romIndex = (newBank * 0x7FFF) + offset;
			return &emulated_cartidge.rom[romIndex];
		}
		else	
			return access_address_from_bank(bank - 0x80, offset);
	}
	//Last bt of rom
	if (bank >= 0xFE && bank <= 0xFF) {
		if (offset > 0xFF00)
			;// TODO reset vectors
		else {
			int romIndex = (0x3D * 0xFFFF) + (((bank-0xFE) * 0xFFFF) + offset);
			return &emulated_cartidge.rom[romIndex];
		}
	}
	return NULL;
}

//Retrieve 2 byte unsigned short from location
uint16_t get2Byte(uint8_t* loc) {
	uint16_t out = loc[1];
	out = out << 8;
	out = out | (0x00FF & loc[0]);
	return out;
}

//Retrieve 4 byte unsigned integer from location
uint32_t get4Byte(uint8_t* loc) {
	uint32_t out = 0;
	out = out | ((0xFF000000 & loc[0]) | (0x00FF0000 & loc[1]) | (0x0000FF00 & loc[2]) | (0x000000FF & loc[3]));
	return out;
}

void store2Byte(uint16_t loc, uint16_t val) {
	system_memory[loc] = (uint8_t)(val & 0xFF00);
	system_memory[loc + 1] = (uint8_t)(val & 0x00FF);
}

void store4Byte(uint16_t loc, uint32_t val) {
	system_memory[loc + 0] = (uint8_t)(val & 0xFF000000);
	system_memory[loc + 1] = (uint8_t)(val & 0x00FF0000);
	system_memory[loc + 2] = (uint8_t)(val & 0x0000FF00);
	system_memory[loc + 3] = (uint8_t)(val & 0x000000FF);
}

uint32_t gen3Byte(uint8_t bank, uint16_t addr) {
	uint32_t output = bank;
	output = bank << 16;
	output = output | addr;
	return output;
}