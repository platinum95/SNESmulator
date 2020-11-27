#pragma once
#include <stdbool.h>

typedef struct ExecutionState {
    uint8_t PBR;
    uint16_t PC;
    uint16_t A;
    uint16_t X;
    uint16_t Y;
    uint16_t SP;
    uint16_t DP;
    uint8_t DB;
    uint8_t pRegister;
    bool emulationMode;
} ExecutionState;

int start_comp();
uint8_t compare( struct ExecutionState A );