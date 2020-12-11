#include "cartridge.h"
#include <stdio.h>
#include <stdlib.h>

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

typedef struct EmulatedCartridge {
    uint8_t*    rom;
    uint8_t*    sram;
    bool        rom_loaded;
    uint32_t    size;
    RomTypes    romType;
} EmulatedCartridge;

EmulatedCartridge emulatedCartridge;

static int ScoreHiROM(const _Bool skip_header) {
    uint8_t    *buf = emulatedCartridge.rom + 0xff00U + 0U + (skip_header ? 0x200U : 0U);
    int        score = 0;

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

    if (emulatedCartridge.size > 1024 * 1024 * 3)
        score += 4;

    if ((1 << (buf[0xd7] - 7)) > 48)
        score -= 1;

    return (score);
}

static int ScoreLoROM(const _Bool skip_header) {
    uint8_t    *buf = emulatedCartridge.rom + 0x7f00 + 0 + (skip_header ? 0x200 : 0);
    int        score = 0;

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

    if (emulatedCartridge.size <= 1024 * 1024 * 16)
        score += 2;

    if ((1 << (buf[0xd7] - 7)) > 48)
        score -= 1;


    return (score);
}

//Load rom from file into dynamically allocated memory
//Returns 0 if success, -1 if rom size incorrect
int cartridgeLoadRom( const char* filepath ) {
    FILE *romFile = fopen( filepath, "rb" );
    if( !romFile ){
        printf( "Failed to open rom file: %s\n", filepath );
        return -1;
    }
    //Get rom size
    fseek( romFile, 0, SEEK_END );
    const long romSize = ftell( romFile );
    fseek( romFile, 0, SEEK_SET );
    if ( romSize <= 0 || romSize > 12580000 )
        return 1;

    emulatedCartridge.rom = malloc( romSize );
    fread( emulatedCartridge.rom, romSize, 1, romFile );    //Read rom into memory
    fclose( romFile );
    emulatedCartridge.rom_loaded = 1;
    emulatedCartridge.size = romSize;

    const int hiScore = ScoreHiROM( 1 );
    const int loScore = ScoreLoROM( 1 );

    emulatedCartridge.romType = loScore > hiScore ? LoRom : HiRom;

    // TODO - fix this bug
    emulatedCartridge.romType = HiRom;
    return 0;
}

void deleteRom() {
    if ( emulatedCartridge.rom_loaded ) {
        free( emulatedCartridge.rom );
    }
}

void cartridgeMemoryAccess( MemoryAddress addressBus, uint8_t *dataBus, bool writeLine ) {
    // TODO
    // Will be based on hi/lo rom score.
    // Just HiRom for now
    // Can probably re-use some of the old code
    if ( writeLine ) {
        // Error?
        printf( "Attempting to write to ROM\n" );
        return;
    }

    if ( addressBus.bank >= 0x40 && addressBus.bank <= 0x7D ) {
        addressBus.bank -= 0x40;
    }
    else if ( addressBus.bank >= 0x80 && addressBus.bank <= 0xFD ) {
        addressBus.bank -= 0x80;
    }
    else if ( addressBus.bank >= 0xFE ) {
        // Bank 0xFE-0xFF
        // Map to bank 0x3E-3F
        addressBus.bank &= 0x3F;
    }
    // TODO - GT says we need to do this, but should verify
    addressBus.bank &= 0x07;
    uint32_t offset = ( (uint32_t)addressBus.bank << 16 ) | ( (uint32_t)addressBus.offset );
    *dataBus = emulatedCartridge.rom[ offset ];
}