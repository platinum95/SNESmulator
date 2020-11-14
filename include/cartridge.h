#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define LO_ROM            0x20
#define HI_ROM            0x21
#define LO_ROM_FASTROM    0x30
#define HI_ROM_FASTROM    0x31
#define EX_LOROM            0x32
#define EX_HIROM            0x35

typedef enum RomTypes {
    HiRom,
    LoRom,
    LoFastRom,
    HiFastRom
} RomTypes;

struct EmulatedCartridge {
    uint8_t*    rom;
    uint8_t*    sram;
    bool        rom_loaded;
    uint32_t    size;
    RomTypes    romType;
};

extern struct EmulatedCartridge emulatedCartridge;

int     LoadRom( const char* filepath );
void    DeleteRom();
int     ScoreHiROM( bool skip_header );
int     ScoreLoROM( bool skip_header );


#endif //CARTRIDGE_H