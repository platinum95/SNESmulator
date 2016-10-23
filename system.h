/*
System emulates the underlying system of the SNES and manages:
	-Startup and initialisation of the components
	-Access to mapped memory and cartridge data
	-Access to components of the system
*/

#pragma once
#ifndef SYSTEM_H
#define SYSTEM_H

//Initialise the system - akin to powering on.
int startup();

//Access point in mapped memory from 24 bit address 
char *access_address(unsigned int addr);

//Load a rom from file
int load_rom(const char* rom_path);

//Begin execution of the loaded cartridge
void begin_execution();

//Access point in memory from a 1 byte bank and a 2 byte offset
char*(*access_address_from_bank)(char bank, short addr);

//Access the start point of the rom data
char *getRomData();


#endif