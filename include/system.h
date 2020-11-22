/*
System emulates the underlying system of the SNES and manages:
    -Startup and initialisation of the components
    -Access to mapped memory and cartridge data
    -Access to components of the system
*/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <endian.h>
#include <stdbool.h>
#include <stdint.h>

// Represents the 24-bit address bus value.
// Note that padding means we don't save space over a uint32_t, but it should
// be easier to access memory components
typedef struct MemoryAddress {
    uint8_t     bank;
    uint16_t    offset;
} MemoryAddress;

//Initialise the system - akin to powering on.
int startup();

//Begin execution of the loaded cartridge
void begin_execution();

// Get a pointer to host memory equivalent to emulated-memory address
uint8_t* snesMemoryMap( MemoryAddress address );

// SNES (both main CPU and SPC700) is little-endian, MSB at highest address.
// On little-endian hosts, we can just treat the memory as-is.
// On big-endian hosts, we need to emulate little-endian storage.
#if __BYTE_ORDER == __LITTLE_ENDIAN
// Read a 2 byte unsigned in host-relative location
static inline uint16_t readU16( uint8_t *loc ) {
    return *(uint16_t*)loc;
}

// Read a 3 byte unsigned in host-relative location, delivered in a 4-byte value
static inline uint32_t readU24( uint8_t *loc ) {
    return ( *(uint32_t*)loc ) & 0x00FFFFFF;
}

// Read a 4 byte unsigned in host-relative location
static inline uint32_t readU32( uint8_t *loc ) {
    return *(uint32_t*)loc;
}

//Store a 2 byte unsigned in host-relative location
static inline void storeU16( uint8_t* loc, uint16_t val ) {
    *( (uint16_t*)loc ) = val;
}

//Store a 4 byte unsigned in host-relative location
static inline void storeU32( uint8_t *loc, uint32_t val ) {
    *( (uint32_t*)loc ) = val;
}

#else
// Read a 2 byte unsigned in host-relative location
static inline uint16_t readU16( uint8_t *loc ) {
    return ( (uint16_t)loc[ 1 ] << 8 ) | (uint16_t)loc[ 0 ];
}

// Read a 3 byte unsigned in host-relative location, delivered in a 4-byte value
static inline uint32_t readU24( uint8_t *loc ) {
        return ( (uint32_t)loc[ 2 ] << 16 )
            | ( (uint32_t)loc[ 1 ] << 8 )
            | ( (uint32_t)loc[ 0 ] );
}

// Read a 4 byte unsigned in host-relative location
static inline uint32_t readU32( uint8_t *loc ) {
    return ( (uint32_t)loc[ 3 ] << 24 ) 
            | ( (uint32_t)loc[ 2 ] << 16 )
            | ( (uint32_t)loc[ 1 ] << 8 )
            | ( (uint32_t)loc[ 0 ] );
}

//Store a 2 byte unsigned in host-relative location
static inline void storeU16( uint8_t *loc, uint16_t val ) {
    loc[ 1 ] = (uint8_t)( ( val >> 8 ) & 0x00FF );
    loc[ 0 ] = (uint8_t)( val & 0x00FF );
}

//Store a 4 byte unsigned in host-relative location
static inline void storeU32( uint8_t *loc, uint32_t val ) {
    loc[ 3 ] = (uint32_t)( ( val >> 24 ) & 0x000000FF );
    loc[ 2 ] = (uint32_t)( ( val >> 16 ) & 0x000000FF );
    loc[ 1 ] = (uint32_t)( ( val >> 8 ) & 0x000000FF );
    loc[ 0 ] = (uint32_t)( val & 0x000000FF );
}
#endif

bool is_reserved(const uint8_t *addr);

#endif