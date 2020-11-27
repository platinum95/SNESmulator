#include "system.h"
#include "cartridge.h"
#include "dsp.h"
#include "spc700.h"
#include "ram.h"
#include "cpu.h"
#include "string.h"

struct address_bus_a {
    uint32_t bus;
    uint8_t RD;
    uint8_t WR;
    uint8_t WRAM;
    uint8_t CART;
};
struct address_bus_b {
    uint8_t bus;
    uint8_t PARD;
    uint8_t PAWR;
};
uint8_t data_bus;
uint8_t open_bus;
#pragma region private_headers

void cycle();
bool execute;
unsigned int cycle_counter;
static uint8_t system_memory[ 0x20000 ];
static uint8_t reserved_memory[ 0x2000 ];
static uint8_t hardware_registers[ 16383 ];

#pragma endregion

bool is_reserved( const uint8_t *addr ){
    return ( addr >= reserved_memory )
             && ( addr < &reserved_memory[ sizeof( reserved_memory ) ] );
}

uint32_t getMappedInstructionAddr(uint8_t bank, uint16_t addr) {
    uint32_t instruction_addr = bank;
    instruction_addr <<= 16;
    instruction_addr |= addr;
    instruction_addr &= ~0x800000;
    return instruction_addr;
}

int startup() {
    InitialiseCpu( emulatedCartridge.romType );
    memset( system_memory, 0x55, 131072 );
    memset( hardware_registers, 0x55, 16383 );
    spc700_initialise();
    memset( reserved_memory, 0x00, 0x1000 );
    memset( &reserved_memory[0x1000], 0x80, 0x1000 );
    dspInitialise();
    return 0;
}

void begin_execution() {
    execute = 1;
    cycle();
}

void cycle() {
    while (execute) {
        if (cycle_counter == 1) {

        }
        else {
            ExecuteNextInstruction();
            spc700_execute_next_instruction();
            dspTick();
        }
        cycle_counter++;
    }
}

uint8_t *accessAddressFromBank_loRom( MemoryAddress address ) {
    if ( address.bank <= 0x3F ) {
        //Shadow ram
        if ( address.offset <= 0x1FFF ) {
            int shadow_addr = 0;
            shadow_addr = shadow_addr | address.offset;
            return &system_memory[shadow_addr];
        }
        //Hardware addresses
        else if ( address.offset <= 0x5FFF ) {
            return &hardware_registers[ address.offset ];
        }
        //Expansion ram
        else if ( address.offset <= 0x7FFF ) {
            return 0;
        }
        //Rom mapping
        else {
            int romIndex = ( address.bank * 0x7FFF ) + address.offset;
            return &emulatedCartridge.rom[ romIndex ];
        }
    }
    //Further rom mapping
    else if ( address.bank <= 0x7C ) {
        int romIndex = (0x3F * 0x7FFF) + ((address.bank * 0xFFFF) + address.offset);
        return &emulatedCartridge.rom[romIndex];
    }
    //Sram
    else if ( address.bank == 0x7D ) {
        return &emulatedCartridge.sram[address.offset];
    }
    //System ram
    else if ( address.bank <= 0x7F ) {
        int ram_addr = (address.bank - 0x7E) * 0xFFFF + address.offset;
        return &system_memory[ram_addr];
    }
    //Fast rom
    else {
        if ( address.offset > 0xFF00 )
            ;// TODO reset vectors
        else
        {
            address.bank -= 0x80;
            return accessAddressFromBank_loRom( address );
        }
    }

    return NULL;
}

#include <stdio.h>

uint8_t *accessAddressFromBank_hiRom( MemoryAddress address ) {
    if ( ( address.bank == 82 || address.bank == 2 || address.bank == 0x7e ) && address.offset == 0 ){
        printf( "bloop" );
    }
    if ( address.bank <= 0x3F ) {
        //Shadow ram
        if ( address.offset <= 0x1FFF ) {
            int shadow_addr = 0;
            shadow_addr = shadow_addr | address.offset;
            return &system_memory[shadow_addr];
        }
        //Hardware addresses
        else if (address.offset <= 0x5FFF ) {
            if ( address.offset >= 0x2140 && address.offset < 0x2144 )
                return accessSpcComPort( address.offset - 0x2140, true ); // TODO
            else
                return &hardware_registers[ address.offset ];
        }
        //sram
        else if ( address.offset <= 0x7FFF ) {
            const uint16_t relative_offset = address.offset - 0x6000;
            if( address.bank <= 0x1F ){
                return &reserved_memory[ relative_offset ];
            } else {
                int sramIndex = (address.bank * 0x1FFF) + relative_offset;
                return &emulatedCartridge.rom[ sramIndex ];
            }
        }
        //Rom mapping
        else {
            //int romIndex = (bank * 0x8000) + (offset - 0x8000);
            //0x01ff70;
            //return &emulatedCartridge.rom[romIndex];
            uint32_t instruction_addr = getMappedInstructionAddr( address.bank, address.offset );
            instruction_addr &= ~0x800000;
            // TODO - add request function to Cartridge source
            return &emulatedCartridge.rom[ instruction_addr ];
        }
    }
    //Further rom mapping
    else if ( address.bank <= 0x7D ) {
        int romIndex =  (((address.bank- 0x40) * 0x10000) + address.offset);
        return &emulatedCartridge.rom[romIndex];
    }
    //System ram
    else if ( address.bank <= 0x7F ) {
        int ram_addr = (address.bank - 0x7E) * 0xFFFF + address.offset -1;
        return &system_memory[ram_addr];
    }
    //Fast rom
    else if ( address.bank <= 0xFD ) {
        address.bank -= 0x80;
        /*
        if (newBank >= 0 && newBank <= 0x3F) {
            int romIndex = (newBank * 0x10000) + offset;
            return &emulatedCartridge.rom[romIndex];
        }
        else    
        */
        return accessAddressFromBank_hiRom( address );
    }
    //Last bt of rom
    else {
        if (address.offset > 0xFF00)
            ;// TODO reset vectors
        else {
            int romIndex = (0x3D * 0xFFFF) + (((address.bank-0xFE) * 0xFFFF) + address.offset);
            // TODO - add request function to Cartridge source
            return &emulatedCartridge.rom[romIndex];
        }
    }
    return NULL;
}

uint8_t* snesMemoryMap( MemoryAddress address ) {
    uint8_t *dataloc;

    // TODO - consider re-pointerifying this function
    if( emulatedCartridge.romType == LoRom ) {
        dataloc = accessAddressFromBank_loRom( address );
    }
    else {
        dataloc = accessAddressFromBank_hiRom( address );
    }

    if ( dataloc == NULL ) {
        return &open_bus;
    }

    open_bus = *dataloc;
    return dataloc;

}

