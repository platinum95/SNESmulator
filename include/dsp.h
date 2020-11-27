#ifndef DSP_H
#define DSP_H

#include <stdbool.h>
#include <stdint.h>

void dspInitialise();
void dspTick();
void accessDspAddressLatch( uint8_t *dataBus, bool writeLine );
void accessDspRegister( uint8_t *dataBus, bool writeLine );

#endif //DSP_H