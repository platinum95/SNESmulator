#include "cartridge.h"
#include <stdio.h>

struct EmulatedCartridge emulatedCartridge;

//Load rom from file into dynamically allocated memory
//Returns 0 if success, -1 if rom size incorrect
int LoadRom( const char* filepath ) {
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

    return 0;
}


void DeleteRom() {
    if (emulatedCartridge.rom_loaded == 1)
        free(emulatedCartridge.rom);
}

int ScoreHiROM(const _Bool skip_header) {
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

int ScoreLoROM(const _Bool skip_header) {
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