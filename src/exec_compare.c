
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



const char* getLine(FILE * file) {
    int n = 78;
    const char* buf = malloc(n);
    const char buf2[3];
    fgets(buf, n, file);
    fgets(buf2, 3, file);
    return buf;
}

int hex_to_int(char hex_char) {
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
        return 10;
        break;
    case 'B':
        return 11;
        break;
    case 'C':
        return 12;
        break;
    case 'D':
        return 13;
        break;
    case 'E':
        return 14;
        break;
    case 'F':
        return 15;
        break;
    default:
        return -1;
        break;

    }
}

uint8_t get8(const char* line, int loc) {
    char characters[2];
    characters[0] = line[loc];
    characters[1] = line[loc + 1];
    uint8_t toRet = (hex_to_int(characters[0]) * 16) + (hex_to_int(characters[1]));
    return toRet;
}

uint16_t get16(const char* line, int loc) {
    char characters[4];
    characters[0] = line[loc];
    characters[1] = line[loc + 1];
    characters[2] = line[loc + 2];
    characters[3] = line[loc + 3];
    int vals[4];
    vals[0] = hex_to_int(characters[0]);
    vals[1] = hex_to_int(characters[1]);
    vals[2] = hex_to_int(characters[2]);
    vals[3] = hex_to_int(characters[3]);
    uint16_t toRet = (vals[0] * 16 * 16 * 16) + (vals[1] * 16 * 16) + (vals[2] * 16) + (vals[3]);
    return toRet;
}

// Parse P-Register values from (ex.) { eNvMxdIzC } -> { 010100101 }
static uint8_t parsePRegister( const char* line, uint8_t *pRegOut ) {

    bool emulation = false;
    uint8_t pRegister = 0x00;

    const char* pText = &line[ P_LOC ];

    for (int i = 0; i < 9; i++) {
        char flag = pText[ i ];
        
        // Igore lower-case
        if ( flag >= 97 && flag <= 122 )
            continue;

        switch ( flag )
        {
        case 'E':
            emulation = true;
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
        }
    }
    *pRegOut = pRegister;
    return emulation;
}

struct execution parseNextLine(FILE *file) {
    const char *line = getLine( file );
//    printf(line);
//    printf("\n");
    struct execution ex;
    ex.program_bank_register = get8( line, PBR_LOC );
    ex.program_counter = get16( line, PC_LOC );
    ex.accumulator = get16( line, A_LOC );
    ex.X = get16( line, X_LOC );
    ex.Y = get16( line, Y_LOC );
    ex.emulation_mode = parsePRegister( line, &ex.p_register );
    return ex;
}

FILE *comp_file;
int start_comp() {
    comp_file = fopen("exec_comp.txt", "r");
    return 0;
}

static const char *byte_to_binary(uint8_t x) {
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
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

static void printState( struct execution state ) {
    char pRegister[ 9 ];
    EncodePRegister( state.p_register, pRegister );

    printf( "\t%02x%04x | A:%04x | X:%04x | Y:%04x | P:%s | E:%i\n",
        state.program_bank_register,
        state.program_counter,
        state.accumulator,
        state.X,
        state.Y,
        pRegister,
        state.emulation_mode );
}  

uint8_t compare( struct execution A ) {
    struct execution B = parseNextLine(comp_file);
    printState( A );
    printState( B );
    
    if (A.program_bank_register != B.program_bank_register)
    {
        printf( "\tProgram Bank mismatch\n" );
        return 1;
    }
    if (A.program_counter != B.program_counter)
    {
        printf( "\tProgram Counter mismatch\n" );
        return 2;
    }
    if (A.accumulator != B.accumulator)
    {
        printf( "\tAccumulator mismatch\n" );
        return 3;
    }
    if (A.X != B.X)
    {
        printf( "\tX Register mismatch\n" );
        return 4;
    }
    if (A.Y != B.Y)
    {
        printf( "\tY Register mismatch\n" );
        return 5;
    }
    if (A.emulation_mode != B.emulation_mode)
    {
        printf( "\tEmulation Mode mismatch\n" );
        return 6;
    }
    if (A.p_register != B.p_register)
    {
        printf( "\tP Register mismatch\n" );
        return 7;
    }

    return 0;

}