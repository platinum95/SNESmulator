#include "dsp.h"
#include <stdio.h>
#include <string.h>

typedef struct __attribute__((__packed__)) VoiceRegisters {
    uint8_t VOL_L;
    uint8_t VOL_R;
    uint8_t Pl;
    uint8_t Ph;
    uint8_t SRCN;
    uint8_t ADSR1;
    uint8_t ADSR2;
    uint8_t GAIN;
    uint8_t ENVX;
    uint8_t OUTX;
} VoiceRegisters;

typedef struct __attribute__((__packed__)) Registers {
    VoiceRegisters Voice0; // 00-09
    uint8_t unusedV0_1[ 2 ]; // 0A-0B
    uint8_t MVOL_L; // 0c
    uint8_t EFB; // 0d
    uint8_t unusedV0_2; // 0E
    uint8_t C0; // 0f
    
    VoiceRegisters Voice1; // 10-19
    uint8_t unusedV1_1[ 2 ]; // 0A-0B
    uint8_t MVOL_R; // 1c
    uint8_t unusedV1_2[ 2 ]; // 0D-0E
    uint8_t C1; // 1f

    VoiceRegisters Voice2; // 20-29
    uint8_t unusedV2_1[ 2 ]; // 0A-0B
    uint8_t EVOL_L; // 2c
    uint8_t PMON; // 2d
    uint8_t unusedV2_2; // 0E
    uint8_t C2; // 2f

    VoiceRegisters Voice3; // 30-39
    uint8_t unusedV3_1[ 2 ]; // 0A-0B
    uint8_t EVOL_R; // 3c
    uint8_t NOV; // 3d
    uint8_t unusedV3_2; // 0E
    uint8_t C3; // 3f

    VoiceRegisters Voice4; // 40-49
    uint8_t unusedV4_1[ 2 ]; // 0A-0B
    uint8_t KON; // 4c
    uint8_t EOV; // 4d
    uint8_t unusedV4_2; // 0E
    uint8_t C4; // 4f

    VoiceRegisters Voice5; // 50-59
    uint8_t unusedV5_1[ 2 ]; // 0A-0B
    uint8_t KOF; // 5c
    uint8_t DIR; // 5d
    uint8_t unusedV5_2; // 0E
    uint8_t C5; // 5f

    VoiceRegisters Voice6; // 60-69
    uint8_t unusedV6_1[ 2 ]; // 0A-0B
    uint8_t FLG; // 6c
    uint8_t ESA; // 6d
    uint8_t unusedV6_2; // 0E
    uint8_t C6; // 6f

    VoiceRegisters Voice7; // 70-79
    uint8_t unusedV7_1[ 2 ]; // 0A-0B
    uint8_t ENDX; // 7c
    uint8_t EDL; // 7d
    uint8_t unusedV7_2; // 0E
    uint8_t C7; // 7f
} Registers;

static Registers registers;

typedef struct VoiceState {
    bool keyOn;
    bool keyOff;
} VoiceState;

typedef struct DspState {
    VoiceState voiceStates[ 8 ];
} DspState;

static DspState dspState;

static uint8_t dspAddressLatch;

void accessDspAddressLatch( uint8_t *dataBus, bool writeLine ) {
    // TODO - maybe add a LIKELY tag here
    if ( writeLine ) {
        dspAddressLatch = *dataBus;
    }
    else {
        *dataBus = dspAddressLatch;
    }
}

static const char* voiceNameLookup[ 0xA ] = {
    "VOL (L)",
    "VOL (R)",
    "P (L)",
    "P (H)",
    "SRCN",
    "ADSR (1)",
    "ADSR (2)",
    "GAIN",
    "ENVX",
    "OUTX"
};

static const char* lowerCNameLookup[ 0x8 ] = {
    "MVOL (L)",
    "MVOL (R)",
    "EVOL (L)",
    "EVOL (R)",
    "KON",
    "KOFF",
    "FLG",
    "ENDX"
};

static const char* lowerDNameLookup[ 0x8 ] = {
    "EFB",
    "N/A (ERROR)",
    "PMON",
    "NON",
    "EON",
    "DIR",
    "ESA",
    "EDL"
};

static inline void printAccess( uint8_t *dataBus, bool writeLine ) {
    uint8_t *hostMemory = ( (uint8_t*) &registers ) + dspAddressLatch;
    if ( writeLine ) {
        printf( "DSP %02x WRTE %02x", dspAddressLatch, *dataBus );
    }
    else {
        printf( "DSP %02x READ %02x", dspAddressLatch, *hostMemory );
    }
    
    // Optional extra info
    uint8_t lowerOffset = dspAddressLatch & 0x0F;
    uint8_t upperOffset = (uint8_t)( ( dspAddressLatch >> 4 ) & 0x00FF );
    if ( lowerOffset < 0xA ) {
        // Voice access
        printf( " -- Voice %i -- %s", upperOffset, voiceNameLookup[ lowerOffset ] );
    }
    else if ( lowerOffset == 0x0F ) {
        // FIR coefficient
        printf( " -- FIR %i", upperOffset );
    }
    else if ( lowerOffset == 0x0C ) {
        printf( " -- %s", lowerCNameLookup[ upperOffset ] );
    }
    else if ( lowerOffset == 0x0D ) {
        printf( " -- %s", lowerDNameLookup[ upperOffset ] );
    }
    else {
        printf( "-- ERROR" );
    }

    printf( "\n" );
}

void accessDspRegister( uint8_t *dataBus, bool writeLine ) {
    uint8_t lowerOffset = dspAddressLatch & 0x0F;
    if ( lowerOffset == 0xA || lowerOffset == 0xB || lowerOffset == 0xE || dspAddressLatch == 0x1D || dspAddressLatch >= 0x80 ) {
        // TODO - maybe signal here
        printf( "Invalid DSP access\n" );
        return;
    }
    printAccess( dataBus, writeLine );
    uint8_t *hostMemory = ( (uint8_t*) &registers ) + dspAddressLatch;

     if ( writeLine ) {
        *hostMemory = *dataBus;
    }
    else {
        *dataBus = *hostMemory;
    }

    return;
}

void dspInitialise() {
    memset( &registers, 0x00, sizeof( Registers ) );
    memset( &dspState, 0x00, sizeof( DspState ) );
    registers.FLG = ( 1 << 7 ) | ( 1 << 6 ); // Reset, Mute
    dspAddressLatch = 0x00;
}

void dspTick() {
    // TODO - This could all probably be done during read/write of registers

    if ( registers.FLG & ( 1 << 7 ) ) {
        // TODO - soft reset
        registers.KON = 0x00;
    }
    uint8_t noiseClockSource = registers.FLG & 0x1F;

    uint16_t sampleDirectoryAddr = registers.DIR * 0x0100;
    uint16_t echoAddr = registers.ESA * 0x0100;

    if ( registers.KON != 0 ) {
        printf( "KON\n" );
    }
    for ( uint8_t voiceId = 0; voiceId < 8; ++voiceId ) {
        
        uint8_t mask = 1 << voiceId;
        bool keyOn = registers.KON & mask;
        if ( !dspState.voiceStates[ voiceId ].keyOn ) {
            if ( !keyOn ) {
                // Nothing to do TODO - verify
                return;
            }
            else {
                // Start playing

            }
        }

        VoiceRegisters *voice = (VoiceRegisters*)( ( (uint8_t*) &registers ) + ( voiceId * 0x10 ) );
        bool keyOff = registers.KOF & mask;
        bool noiseOn = registers.NOV & mask;
        bool pmodOn = ( registers.PMON & 0xFE ) & mask;
        bool echoOn = ( registers.EOV & 0xFE ) & mask;

        bool adsrEnable = voice->ADSR1 & 0x80;
        uint8_t decayRate = voice->ADSR1 & 0x0F;
        uint8_t attackRate = ( voice->ADSR1 >> 4 ) & 0x07;
        uint8_t sustainRate = voice->ADSR2 & 0x1F;
        uint8_t sustainLevel = ( voice->ADSR2 >> 5 ) & 0x07;

        uint8_t gainParameter;
        if ( ( voice->GAIN & 0x80 ) == 0x00 ) {
            // Direct mode
            gainParameter = voice->GAIN & 0xEF;
        }
        else {
            // TODO - detect increase|decrease (linear|bent line |exponential) modes
            gainParameter = voice->GAIN & 0x1F;
        }
        
        voice->ENVX &= 0xEF;
        
        int8_t outX = (int8_t)voice->OUTX;

    }
}
