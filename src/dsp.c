#include "dsp.h"
#include "spc700.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <portaudio.h>


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

typedef struct __attribute__((__packed__)) VoiceRegisters {
    int8_t VOL_L;
    int8_t VOL_R;
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


static uint8_t dspAddressLatch;

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

    if ( dspAddressLatch == 0x4C && *dataBus != 0 && writeLine ) {
        printf( "bloop\n" );
    }

    printf( "\n" );
}


// KEY States
#define KEY_STATE_ALL_OFF   0x00
#define KEY_STATE_KON       0x02
#define KEY_STATE_KON_KOFF  0x03
#define KEY_STATE_KOF       0x01

typedef struct VoiceData {
    bool leadinLoopFlag;
    bool loopLoopFlag;
    uint16_t numLeadinBlocks;
    uint16_t numLoopBlocks;
    int16_t *leadinSamples;
    int16_t *loopSamples;
} VoiceData;

typedef struct VoiceState {
    bool endxSet;
    uint8_t currentState;
    bool playing;

    uint16_t currentBlock;
    uint8_t currentSample;
    VoiceData voiceData;
} VoiceState;

typedef struct DspState {
    VoiceState voiceStates[ 8 ];
} DspState;

static DspState dspState;


void accessDspAddressLatch( uint8_t *dataBus, bool writeLine ) {
    // TODO - maybe add a LIKELY tag here
    if ( writeLine ) {
        dspAddressLatch = *dataBus;
    }
    else {
        *dataBus = dspAddressLatch;
    }
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
static PaStream *portAudioStream;

int portAudioStreamCallback( const void *input, void *output, unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags, void *userData );

void dspInitialise() {
    memset( &registers, 0x00, sizeof( Registers ) );
    memset( &dspState, 0x00, sizeof( DspState ) );
    registers.FLG = ( 1 << 7 ) | ( 1 << 6 ); // Reset, Mute
    dspAddressLatch = 0x00;

    if ( Pa_Initialize() != paNoError ) {
        printf( "Failed to initialise PortAudio\n" );
    }

    if ( Pa_OpenDefaultStream( &portAudioStream, 0, 2, paInt16, 32000, 1, portAudioStreamCallback, NULL ) != paNoError ) {
        printf( "Failed to open PortAudio stream\n" );
    }

    if ( Pa_StartStream( portAudioStream ) != paNoError ) {
        printf( "Failed to start PortAudio stream\n" );
    }


    for( uint8_t i = 0; i < 8; ++i ) {
        dspState.voiceStates[ i ].endxSet = false;
        dspState.voiceStates[ i ].currentState = KEY_STATE_ALL_OFF;
        dspState.voiceStates[ i ].playing = false;
        dspState.voiceStates[ i ].voiceData.leadinSamples = NULL;
        dspState.voiceStates[ i ].voiceData.loopSamples = NULL;
        dspState.voiceStates[ i ].voiceData.leadinLoopFlag = false;
        dspState.voiceStates[ i ].voiceData.loopLoopFlag = false;
        dspState.voiceStates[ i ].voiceData.numLeadinBlocks = 0;
        dspState.voiceStates[ i ].voiceData.numLoopBlocks = 0;
        dspState.voiceStates[ i ].currentBlock = 0;
        dspState.voiceStates[ i ].currentSample = 0;
    }

}

static inline uint16_t getBRRBlockCount( uint16_t addr ) {
    static const uint16_t s_maxBlocks = 256; // TODO
        
    uint16_t numBlocks = 0;
    const size_t blockSize = 9;
    while( numBlocks <= s_maxBlocks ) {
        uint8_t header = spcMemoryMapRead( addr + ( numBlocks * blockSize ) );
        ++numBlocks;
        if ( header & 0x01 ) {
            // End-block reached
            break;
        }
    }
    if ( numBlocks == s_maxBlocks ) {
        // TODO - error
        printf( "Couldn't find ending block\n" );
    }

    return numBlocks;
}

// Returns true if last BRR block had loop-flag set.
// Buffer must be pre-allocated
static bool decodeBRRChain( const uint16_t addr, int16_t *sampleBuffer ) {
    static const float filterCoeffMap[4][2] = {
        { 0.0f, 0.0f },
        { 0.9375f, 0.0f },
        { 1.90625f,  -0.9375f },
        { 1.796875f, -0.8125f }
    };
    bool endBlock = false;
    bool loopSet = false;
    uint16_t blockIdx = 0;
    const size_t blockSize = 9;
    while( !endBlock ) {
        // Read the 9-byte block
        uint16_t blockAddrOffset = addr + ( blockIdx * blockSize );
        uint8_t head = spcMemoryMapRead( blockAddrOffset++ );

        endBlock = ( head & 0x01 );
        loopSet = ( head & 0x02 );
        uint8_t sampleFilter = ( head >> 2 ) & 0x03;
        uint8_t shiftRange = ( head >> 4 ) & 0x0F;

        if ( sampleFilter > 3 ) {
            // TODO - some error
            printf( "Invalid block filter\n" );
        }
        const float *filterCoeffs = filterCoeffMap[ sampleFilter ];
        int16_t lastSamples[ 2 ] = { 0, 0 };

        for ( uint8_t j = 0; j < 8; ++j ) {
            const uint16_t sampleIdx = ( blockIdx * 16 ) + ( j * 2 );
            uint8_t brrByte = spcMemoryMapRead( blockAddrOffset++ );
            
            // High-byte
            uint8_t brrSample = ( brrByte >> 4 ) & 0x0F;
            int16_t sampleRaw = ( ( ( ( brrSample & 0x08 ) ? 0xFFF0 : 0x0000 ) | brrSample ) << shiftRange ) >> 1;
            int16_t sampleFiltered = sampleRaw + ( lastSamples[ 0 ] * filterCoeffs[ 0 ] ) + ( lastSamples[ 1 ] * filterCoeffs[ 1 ] );
            lastSamples[ 1 ] = lastSamples[ 0 ];
            lastSamples [ 0 ] = sampleFiltered;
            sampleBuffer[ sampleIdx ] = sampleFiltered * 100;

            // low-byte
            brrSample = brrByte & 0x0F;
            sampleRaw = ( ( ( ( brrSample & 0x08 ) ? 0xFFF0 : 0x0000 ) | brrSample ) << shiftRange ) >> 1;
            sampleFiltered = sampleRaw + ( lastSamples[ 0 ] * filterCoeffs[ 0 ] ) + ( lastSamples[ 1 ] * filterCoeffs[ 1 ] );
            lastSamples[ 1 ] = lastSamples[ 0 ];
            lastSamples [ 0 ] = sampleFilter;
            sampleBuffer[ sampleIdx + 1 ] = sampleFiltered * 100;
        }
        ++blockIdx;
    }
    return loopSet;
}

static inline void loadVoiceBuffer( uint8_t voiceId ) {
    VoiceState *voiceState = &dspState.voiceStates[ voiceId ];
    VoiceRegisters *voice = (VoiceRegisters*)( ( (uint8_t*) &registers ) + ( voiceId * 0x10 ) );
    const uint16_t sampleDirectoryAddr = registers.DIR * 0x0100;

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

    // Copy over the sound data
    uint16_t sourceAddr = sampleDirectoryAddr + ( voice->SRCN * 2 * sizeof( uint16_t ) );
    uint16_t sampleStartAddr = spcMemoryMapReadU16( sourceAddr );
    uint16_t sampleLoopAddr = spcMemoryMapReadU16( sourceAddr + sizeof( uint16_t ) );

    voiceState->voiceData.numLeadinBlocks = getBRRBlockCount( sampleStartAddr );
    size_t leadinBufferSize = voiceState->voiceData.numLeadinBlocks * 16 * sizeof( int16_t ); // Each block has 16 PCM16 samples in it
    if ( voiceState->voiceData.leadinSamples != NULL ) {
        free( voiceState->voiceData.leadinSamples );
    }

    voiceState->voiceData.leadinSamples = (int16_t*) malloc( leadinBufferSize );
    voiceState->voiceData.leadinLoopFlag = decodeBRRChain( sampleStartAddr, voiceState->voiceData.leadinSamples );

    voiceState->voiceData.numLoopBlocks = getBRRBlockCount( sampleLoopAddr );
    size_t loopBufferSize = voiceState->voiceData.numLoopBlocks * 16 * sizeof( int16_t ); // Each block has 16 PCM16 samples in it
    if ( voiceState->voiceData.loopSamples != NULL ) {
        free( voiceState->voiceData.loopSamples );
    }

    voiceState->voiceData.loopSamples = (int16_t*) malloc( loopBufferSize );
    voiceState->voiceData.loopLoopFlag = decodeBRRChain( sampleLoopAddr, voiceState->voiceData.loopSamples );

    //uint16_t pitch = ( ( (uint16_t)voice->Ph & 0x3F ) << 8 ) | voice->Pl;
    //uint32_t sampleRate = pitch * 7.8125;
}

typedef struct VoiceSample {
    int16_t sampleLeft;
    int16_t sampleRight;
} VoiceSample;

// Called from audio CB so needs to be fast
static VoiceSample getNextVoiceSample( uint8_t voiceId ) {
    // TODO - should probably be locked
    // TODO - optimise

    VoiceState *voiceState = &dspState.voiceStates[ voiceId ];
    if ( !voiceState->playing ) {
        return (VoiceSample){ 0, 0 };
    }

    VoiceRegisters *voice = (VoiceRegisters*)( ( (uint8_t*) &registers ) + ( voiceId * 0x10 ) );

    // This is pretty inefficient...
    int16_t sample = 0;
    const bool inLoopBlock = voiceState->currentBlock >= voiceState->voiceData.numLeadinBlocks;
    if ( inLoopBlock ) {
        uint16_t loopBlock = voiceState->currentBlock - voiceState->voiceData.numLeadinBlocks;
        uint16_t loopSample = ( loopBlock * 16 ) + voiceState->currentSample++;
        if ( voiceState->currentSample > 16 ) {
            voiceState->currentSample = 0;
            voiceState->currentBlock++;
            if ( loopBlock >= voiceState->voiceData.numLoopBlocks ) {
                voiceState->currentBlock = voiceState->voiceData.numLeadinBlocks;
            }
        }
        sample = voiceState->voiceData.loopSamples[ loopSample ];
    }
    else {
        uint16_t leadinSample = ( voiceState->currentBlock * 16 ) + voiceState->currentSample++;
        if ( voiceState->currentSample > 16 ) {
            voiceState->currentSample = 0;
            voiceState->currentBlock++;
            if ( voiceState->currentBlock >= voiceState->voiceData.numLeadinBlocks ) {
                // TODO - check if we should loop, set envelope if we're not
                // TODO - set ENDX
                registers.ENDX |= 1 << voiceId;
            }
        }
        sample = voiceState->voiceData.leadinSamples[ leadinSample ];
    }

    // TODO - shouldn't be writing directly to registers from this thread
    voice->OUTX = (uint8_t)( sample >> 8 & 0x00FF );

    return (VoiceSample){ 
        (int16_t)( ( (int32_t)sample * (int32_t)voice->VOL_L ) >> 7 ), 
        (int16_t)( ( (int32_t)sample * (int32_t)voice->VOL_R ) >> 7 )
    };
}

void dspTick() {
    // TODO - This could all probably be done during read/write of registers

    if ( registers.FLG & ( 1 << 7 ) ) {
        // TODO - soft reset
        registers.KON = 0x00;
    }
    uint8_t noiseClockSource = registers.FLG & 0x1F;

    
    uint16_t echoAddr = registers.ESA * 0x0100;

    for ( uint8_t voiceId = 0; voiceId < 8; ++voiceId ) {
        
        uint8_t mask = 1 << voiceId;
        bool keyOn = registers.KON & mask;
        bool keyOff = registers.KOF & mask;
        uint8_t newKeyState = ( keyOn ? 0x02 : 0x00 ) | ( keyOff ? 0x01 : 0x00 );
        
        VoiceState *voiceState = &dspState.voiceStates[ voiceId ];
        uint8_t currentKeyState = voiceState->currentState;// ( voiceState->kOn ? 0x02 : 0x00 ) | ( voiceState->kOff ? 0x01 : 0x00 );
        if ( newKeyState != currentKeyState ) {
            // Changing states

            if ( ( currentKeyState == 0x00 && newKeyState == 0x01 )
                || ( currentKeyState == 0x01 && newKeyState == 0x00 )
                || ( currentKeyState == 0x01 && newKeyState == 0x03 )
                || ( currentKeyState == 0x03 && newKeyState == 0x01 ) ) {
                // Do-nothing state transitions
                voiceState->playing = false;
            }
            else if ( ( currentKeyState == 0x03 && newKeyState == 0x02 ) ) {
                // KON|KOFF -> KON|koff
                // TODO - if within N cycles of kon->KON, then play. For now, ignore that quirk
                voiceState->playing = false;
            }
            else if ( currentKeyState == 0x00 && newKeyState == 0x02 ) {
                if ( voiceState->playing ) {
                    // Reset voice
                    
                }
                else {
                    // Playing from silence
                    voiceState->playing = true;
                    loadVoiceBuffer( voiceId );
                }
            }
            else if ( currentKeyState == 0x02 && newKeyState == 0x00 ) {
                // KON cleared but KOFF not set. Don't change Playing state
            }
            else {
                // TODO - error
                printf( "Invalid state transition\n" );
            }
            
            voiceState->currentState = newKeyState;
            // Reset KON flag for this voice
            registers.KON &= ~mask;
        }        
        if ( voiceState->playing ){
            // Update playing state
        }
        
        // bool keyOff = registers.KOF & mask;
        // bool noiseOn = registers.NOV & mask;
        // bool pmodOn = ( registers.PMON & 0xFE ) & mask;
        // bool echoOn = ( registers.EOV & 0xFE ) & mask;
    }
}

int portAudioStreamCallback( const void *input, void *output, unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags, void *userData ) {
    (void)input;
    (void)frameCount;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    static uint64_t cnt = 0;
    VoiceSample frameSample = { 0x00, 0x00 };
    int16_t *outBuffer = (int16_t*) output;
    for ( uint8_t voiceId = 0; voiceId < 8; ++voiceId ) {
        VoiceSample voiceSample = getNextVoiceSample( voiceId );
        frameSample.sampleLeft += voiceSample.sampleLeft;
        frameSample.sampleRight += voiceSample.sampleRight;
    }

    //frameSample = ( ++cnt % 256 ) * 10;
    frameSample.sampleLeft = (int16_t)( ( (int32_t)frameSample.sampleLeft * (int32_t)registers.MVOL_L ) >> 7 );
    frameSample.sampleRight = (int16_t)( ( (int32_t)frameSample.sampleRight * (int32_t)registers.MVOL_R ) >> 7 );
    *outBuffer++ = frameSample.sampleLeft;
    *outBuffer = frameSample.sampleRight;
    return paNoError;
}