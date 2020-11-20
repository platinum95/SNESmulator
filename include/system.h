/*
System emulates the underlying system of the SNES and manages:
    -Startup and initialisation of the components
    -Access to mapped memory and cartridge data
    -Access to components of the system
*/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>


//Initialise the system - akin to powering on.
int startup();

//Access point in mapped memory from 24 bit address 
uint8_t *access_address( unsigned int addr );

//Begin execution of the loaded cartridge
void begin_execution();

//Access point in memory from a 1 byte bank and a 2 byte offset
uint8_t* accessAddressFromBank( uint8_t bank, uint16_t addr );

//Access the start point of the rom data
uint8_t *getRomData();

//Access the start point of system ram
uint8_t *accessSystemRam();

//Retrieve 2 byte unsigned short from location
uint16_t get2Byte(uint8_t* loc);

//Retrieve 4 byte unsigned integer from location
uint32_t get3Byte(uint8_t* loc);

//Retrieve 4 byte unsigned integer from location
uint32_t get4Byte(uint8_t* loc);

//Store a 2 byte unsigned short in location, emulation mapped
void store2Byte(uint16_t loc, uint16_t val);

//Store a 4 byte unsigned short in location, emulation mapped
void store4Byte(uint32_t loc, uint32_t val);

//Store a 2 byte unsigned short in local space
void store2Byte_local(uint8_t* loc, uint16_t val);

//Generate a 3 byte value
uint32_t gen3Byte(uint8_t bank, uint16_t addr);

uint32_t getMappedInstructionAddr(uint8_t bank, uint16_t addr);

_Bool is_reserved(const uint8_t *addr);

#endif