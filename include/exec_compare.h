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

typedef enum ComparisonResult {
    Match = 0,
    ProgramBankMismatch = 1 << 0,
    ProgramCounterMismatch = 1 << 1,
    AccumulatorMismatch = 1 << 2,
    XMismatch = 1 << 3,
    YMismatch = 1 << 4,
    EmulationModeMismatch = 1 << 5,
    PRegisterMismatch = 1 << 6,
    StackPointerMismatch = 1 << 7,
    DataBankMismatch = 1 << 8,
    DirectPageMismatch = 1 << 9
} ComparisonResult;

int start_comp();
ComparisonResult compare( struct ExecutionState A );