#ifndef CORE_65816_H
#define CORE_65816_H

#include <stdbool.h>

void coreInitialise();
void coreTick();

void coreIRQ( bool level );
void coreNMI( bool level );

void executeIRQ();
void executeNMI();

#endif// CORE_65816_H