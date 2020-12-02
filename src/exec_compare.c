
#include "cpu.h"
#include "exec_compare.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PBR_LOC 1
#define PC_LOC 4
#define A_LOC 47
#define X_LOC 54
#define Y_LOC 61
#define P_LOC 68

#define CARRY_FLAG  0x1
#define ZERO_FLAG 0x2
#define INTERRUPT_FLAG 0x04
#define DECIMAL_FLAG 0x08
#define OVERFLOW_FLAG 0x40
#define NEGATIVE_FLAG 0x80
#define M_FLAG 0x20
#define X_FLAG 0x10



void getLine( FILE * file, char buf[ 0x100 ] ) {
    int n = 0x100;
    fgets( buf, n, file );
}

uint8_t hex_to_int( char hex_char ) {
    switch (hex_char) {
    case '0':
        return 0;
        break;
    case '1':
        return 1;
        break;
    case '2':
        return 2;
        break;
    case '3':
        return 3;
        break;
    case '4':
        return 4;
        break;
    case '5':
        return 5;
        break;
    case '6':
        return 6;
        break;
    case '7':
        return 7;
        break;
    case '8':
        return 8;
        break;
    case '9':
        return 9;
        break;
    case 'A':
    case 'a':
        return 10;
        break;
    case 'B':
    case 'b':
        return 11;
        break;
    case 'C':
    case 'c':
        return 12;
        break;
    case 'D':
    case 'd':
        return 13;
        break;
    case 'E':
    case 'e':
        return 14;
        break;
    case 'F':
    case 'f':
        return 15;
        break;
    default:
        return -1;
        break;

    }
}

uint8_t get8( const char* line ) {
    uint8_t MSN = hex_to_int( line[ 0 ] );
    uint8_t LSN = hex_to_int( line[ 1 ] );
    return ( MSN << 4 ) | LSN;
}

uint16_t get16( const char* line ) {
    uint16_t MSB = (uint16_t)get8( line );;
    uint16_t LSB = (uint16_t)get8( line + 2 );
    return ( MSB << 8 ) | LSB;
}

// Parse P-Register values from (ex.) { eNvMxdIzC } -> { 010100101 }
static uint8_t parsePRegister( const char* line, bool *emulationMode ) {
    uint8_t pRegister = 0x00;
    *emulationMode = false;

    for ( uint8_t i = 0; i < 8; ++i ) {
        char flag = line[ i ];
        switch ( flag ) {
        case 'E':
        case '1':
        case 'B':
            pRegister |= X_FLAG;
            pRegister |= M_FLAG;
            *emulationMode = true;
            break;
        case 'N':
            pRegister |= NEGATIVE_FLAG;
            break;
        case 'V':
            pRegister |= OVERFLOW_FLAG;
            break;
        case 'M':
            pRegister |= M_FLAG;
            break;
        case 'X':
            pRegister |= X_FLAG;
            break;
        case 'D':
            pRegister |= DECIMAL_FLAG;
            break;
        case 'I':
            pRegister |= INTERRUPT_FLAG;
            break;
        case 'Z':
            pRegister |= ZERO_FLAG;
            break;
        case 'C':
            pRegister |= CARRY_FLAG;
            break;
        case '.':
        default:
            break;
        }
    }
    return pRegister;
}
/*
00ff70 sei                     A:0000 X:0000 Y:0000 S:01ff D:0000 DB:00 ..1B.I.. V:  0 H: 46 F: 0 C:      186
*/
ExecutionState parseNextLine( FILE *file ) {
    char line[ 0x100 ];
    getLine( file, line );

    ExecutionState ex;
    ex.PBR = get8( line + 0 );
    ex.PC = get16( line + 2 );
    ex.A = get16( line + 33 );
    ex.X = get16( line + 40 );
    ex.Y = get16( line + 47 );
    ex.SP = get16( line + 54 );
    ex.DP = get16( line + 61 );
    ex.DB = get8( line + 69 );
    ex.pRegister = parsePRegister( line + 72, &ex.emulationMode );
    return ex;
}

FILE *comp_file;
int start_comp() {
    comp_file = fopen("exec_comp_1m.txt", "r");
    return 0;
}

static void EncodePRegister( uint8_t regValue, char outBuffer[ 9 ] ) {
    outBuffer[ 0 ] = regValue & NEGATIVE_FLAG ? 'N' : 'n';
    outBuffer[ 1 ] = regValue & OVERFLOW_FLAG ? 'V' : 'v';
    outBuffer[ 2 ] = regValue & M_FLAG  ? 'M' : 'm';
    outBuffer[ 3 ] = regValue & X_FLAG ? 'X' : 'x';
    outBuffer[ 4 ] = regValue & DECIMAL_FLAG ? 'D' : 'd';
    outBuffer[ 5 ] = regValue & INTERRUPT_FLAG ? 'I' : 'i';
    outBuffer[ 6 ] = regValue & ZERO_FLAG ? 'Z' : 'z';
    outBuffer[ 7 ] = regValue & CARRY_FLAG ? 'C' : 'c';
    outBuffer[ 8 ] = '\0';
}

static void printExecutionState( const ExecutionState *state ) {
    char pRegister[ 9 ];
    EncodePRegister( state->pRegister, pRegister );

    printf( "\t%02x%04x | A:%04x | X:%04x | Y:%04x | DP:%04x | DB:%02x | SP:%04x | P:%s | E:%i\n",
        state->PBR,
        state->PC,
        state->A,
        state->X,
        state->Y,
        state->DP,
        state->DB,
        state->SP,
        pRegister,
        state->emulationMode );
}

ComparisonResult compareStates( const ExecutionState *groundTruth, const ExecutionState *currentState ) {
    ComparisonResult result = Match;
    if ( currentState->PBR != groundTruth->PBR )
    {
        printf( "\tProgram Bank mismatch\n" );
        result |= ProgramBankMismatch;
    }
    if ( currentState->PC != groundTruth->PC )
    {
        printf( "\tProgram Counter mismatch\n" );
        result |= ProgramCounterMismatch;
    }
    if ( currentState->A != groundTruth->A )
    {
        printf( "\tAccumulator mismatch\n" );
        result |= AccumulatorMismatch;
    }
    if ( currentState->X != groundTruth->X )
    {
        printf( "\tX Register mismatch\n" );
        result |= XMismatch;
    }
    if ( currentState->Y != groundTruth->Y )
    {
        printf( "\tY Register mismatch\n" );
        result |= YMismatch;
    }
    if ( currentState->emulationMode != groundTruth->emulationMode )
    {
        printf( "\tEmulation Mode mismatch\n" );
        result |= EmulationModeMismatch;
    }
    if ( currentState->pRegister != groundTruth->pRegister )
    {
        printf( "\tP Register mismatch\n" );
        result |= PRegisterMismatch;
    }
    if ( currentState->SP != groundTruth->SP )
    {
        printf( "\tSP mismatch\n" );
        result |= StackPointerMismatch;
    }
    if ( currentState->DB != groundTruth->DB )
    {
        printf( "\tDB mismatch\n" );
        result |= DataBankMismatch;
    }
    if ( currentState->DP != groundTruth->DP )
    {
        printf( "\tDP mismatch\n" );
        result |= DirectPageMismatch;
    }
    return result;
}

// On PC mismatch, look ahead in ground truth data to attempt to resync.
// Return true if resync successful, false otherwise
static bool attemptResync( const ExecutionState *targetState ) {
    printf( "Attempting resync...\n" );
    for ( uint8_t i = 0; i < 60; ++i ) {
        ExecutionState groundTruth = parseNextLine( comp_file );
        if ( compareStates( &groundTruth, targetState ) == 0 ) {
            printf( "Found matching state at +%i\n", i );
            printExecutionState( &groundTruth );
            return true;
        }
    }
    printf( "Failed to find matching state\n" );
    return false;
}

ComparisonResult compare( ExecutionState internalState ) {
    ExecutionState groundTruth = parseNextLine( comp_file );
    printExecutionState( &internalState );
    printExecutionState( &groundTruth );

    uint8_t comparison = compareStates( &groundTruth, &internalState );
    if ( ( comparison & ProgramCounterMismatch ) && attemptResync( &internalState ) ) {
        comparison = Match;
    }
    
    return comparison;
}