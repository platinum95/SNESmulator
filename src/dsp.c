#include "dsp.h"
#include "spc700.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <AL/al.h>
#include <AL/alc.h>

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

// KEY States
#define KEY_STATE_ALL_OFF   0x00
#define KEY_STATE_KON       0x02
#define KEY_STATE_KON_KOFF  0x03
#define KEY_STATE_KOF       0x01

typedef struct VoiceState {
    ALuint voiceSource;
    ALuint audioBuffers[ 3 ]; // 2 lead-in buffers, 1 loop buffer
    bool endxSet;
    uint8_t currentState;
    bool playing;
    bool playingLoop;
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

    // Setup openAL
    ALCdevice *openAlDevice = alcOpenDevice( NULL );
    if ( !openAlDevice ) {
        printf( "Failed to set up openAl device\n" );
    }

    ALCcontext *openAlContext = alcCreateContext( openAlDevice, NULL );
    if( !openAlContext ) {
        printf( "Failed to set up OpenAL Context\n" );
    }

    ALCboolean contextMadeCurrent = alcMakeContextCurrent( openAlContext );
    if( contextMadeCurrent != ALC_TRUE ) {
        printf( "Failed to make OpenAL Context current\n" );
    }

    for( uint8_t i = 0; i < 8; ++i ) {
       // alGenBuffers( 3, dspState.voiceStates[ i ].audioBuffers );
       // alGenSources( 1, &dspState.voiceStates[ i ].voiceSource );
        dspState.voiceStates[ i ].endxSet = false;
        dspState.voiceStates[ i ].currentState = KEY_STATE_ALL_OFF;
        dspState.voiceStates[ i ].playing = false;
        dspState.voiceStates[ i ].playingLoop = false;
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

    // Copy over the sound data
    uint16_t sourceAddr = sampleDirectoryAddr + ( voice->SRCN * 2 * sizeof( uint16_t ) );
    uint16_t sampleStartAddr = spcMemoryMapReadU16( sourceAddr );
    uint16_t sampleLoopAddr = spcMemoryMapReadU16( sourceAddr + sizeof( uint16_t ) );
    sampleStartAddr = sampleLoopAddr;
    uint16_t numBlocksLeadin = getBRRBlockCount( sampleStartAddr );
    uint16_t numBlocksLoop = getBRRBlockCount( sampleLoopAddr );

    size_t leadinBufferSize = numBlocksLeadin * 16 * sizeof( int16_t ); // Each block has 16 PCM16 samples in it
    int16_t *leadinSampleBuffer = (int16_t*) malloc( leadinBufferSize );
    voiceState->playingLoop = decodeBRRChain( sampleStartAddr, leadinSampleBuffer );

    uint16_t pitch = ( ( (uint16_t)voice->Ph & 0x3F ) << 8 ) | voice->Pl;
    uint32_t sampleRate = pitch * 7.8125;

    // 0 -> N-1 BRR blocks into the first buffer
    uint8_t numBuffers = 2;
    uint16_t initBufferSize = leadinBufferSize - ( 16 * sizeof( int16_t ) );
    ALenum format = AL_FORMAT_MONO16;
    
    ALuint tBuffer;
    alGenBuffers( 1, &tBuffer );
    alBufferData( tBuffer, format, leadinSampleBuffer, leadinBufferSize, sampleRate );
 //   alBufferData( voiceState->audioBuffers[ 0 ], AL_FORMAT_MONO16, leadinSampleBuffer, initBufferSize, sampleRate );
 //   alBufferData( voiceState->audioBuffers[ 1 ], AL_FORMAT_MONO16, ( (uint8_t*)leadinSampleBuffer ) + initBufferSize, 16 * sizeof( int16_t ), sampleRate );
 //   if ( voiceState->playingLoop ) {
 //       uint16_t numBlocksLoop = getBRRBlockCount( sampleLoopAddr );
 //       uint16_t loopBufferSize = numBlocksLoop * 16 * sizeof( int16_t ); // Each block has 16 PCM16 samples in it
 //       int16_t *loopSampleBuffer = (int16_t*) malloc( loopBufferSize ); 
 //       bool loopHasLoop = decodeBRRChain( sampleLoopAddr, loopSampleBuffer );
 //       alBufferData( voiceState->audioBuffers[ 2 ], AL_FORMAT_MONO16, loopSampleBuffer, loopBufferSize, sampleRate );
 //       ++numBuffers;
 //   }
    // Queue 'em up
    ALuint source = voiceState->voiceSource;
    alGenSources( 1, &source );

    alSourcef( source, AL_PITCH, 1 );
    alSourcef( source, AL_GAIN, 1.0f );
    alSource3f( source, AL_POSITION, 0, 0, 0 );
    alSource3f( source, AL_VELOCITY, 0, 0, 0 );
    alSourcei( source, AL_LOOPING, AL_TRUE );
    //alSourceQueueBuffers( source, numBuffers, voiceState->audioBuffers );
    alSourcei( source, AL_BUFFER, tBuffer );
    alSourcePlay( source );
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
            ALint buffersProcessed;
            alGetSourcei( voiceState->voiceSource, AL_BUFFERS_PROCESSED, &buffersProcessed );
            if ( buffersProcessed == 1 && !voiceState->endxSet ) {
                // First buffer complete, set ENDX flag
                registers.ENDX |= mask;
                voiceState->endxSet = true;
            }
            else if ( buffersProcessed == 2 && voiceState->playingLoop ) {
                // In the loop buffer. Unqueue the first 2 buffers and set source to loop
                // TODO - that
            }

        }
        
        // bool keyOff = registers.KOF & mask;
        // bool noiseOn = registers.NOV & mask;
        // bool pmodOn = ( registers.PMON & 0xFE ) & mask;
        // bool echoOn = ( registers.EOV & 0xFE ) & mask;

        // bool adsrEnable = voice->ADSR1 & 0x80;
        // uint8_t decayRate = voice->ADSR1 & 0x0F;
        // uint8_t attackRate = ( voice->ADSR1 >> 4 ) & 0x07;
        // uint8_t sustainRate = voice->ADSR2 & 0x1F;
        // uint8_t sustainLevel = ( voice->ADSR2 >> 5 ) & 0x07;

        // uint8_t gainParameter;
        // if ( ( voice->GAIN & 0x80 ) == 0x00 ) {
        //     // Direct mode
        //     gainParameter = voice->GAIN & 0xEF;
        // }
        // else {
        //     // TODO - detect increase|decrease (linear|bent line |exponential) modes
        //     gainParameter = voice->GAIN & 0x1F;
        // }
        
        // voice->ENVX &= 0xEF;
        
        // int8_t outX = (int8_t)voice->OUTX;
    }
}