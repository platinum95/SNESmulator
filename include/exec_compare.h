#pragma once

struct execution {
    uint8_t program_bank_register;
    uint16_t program_counter;

    uint16_t accumulator;
    uint16_t X, Y;
    uint8_t p_register;
    uint8_t emulation_mode;
};

int start_comp();

uint8_t compare(struct execution A);