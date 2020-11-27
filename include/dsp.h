#ifndef DSP_H
#define DSP_H

#include <stdbool.h>
#include <stdint.h>

void dspInitialise();
void dspTick();
uint8_t *accessDspRegister( uint8_t reg, bool writeLine );

#endif //DSP_H