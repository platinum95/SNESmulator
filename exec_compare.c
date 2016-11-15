
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "exec_compare.h"

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

uint8_t parseP(const char* line, uint8_t *p_reg) {
	uint8_t emulation, p_parsed = 0;
	const char* p_start = &line[P_LOC];
	if (p_start[0] == 'e')
		emulation = 0;
	else
		emulation = 1;
	p_start = &line[P_LOC + 1];

	for (int i = 0; i < 8; i++) {
		if (p_start[i] < 93) {
			p_parsed |= (0x01 << (7 - i));
		}
		else {
			p_parsed += 0;
		}
	}
	*p_reg = p_parsed;
	return emulation;
}

struct execution parseNextLine(FILE *file) {
	const char *line = getLine(file);
	printf(line);
	printf("\n");
	struct execution ex;
	ex.program_bank_register = get8(line, PBR_LOC);
	ex.program_counter = get16(line, PC_LOC);
	ex.accumulator = get16(line, A_LOC);
	ex.X = get16(line, X_LOC);
	ex.Y = get16(line, Y_LOC);
	ex.emulation_mode = parseP(line, &ex.p_register);
	return ex;
}

FILE *comp_file;
int start_comp() {
	comp_file = fopen("export_exec.txt", "r");
	return 0;
}

uint8_t compare(struct execution A) {
	struct execution B = parseNextLine(comp_file);
	if (A.program_bank_register != B.program_bank_register)
		return 1;
	if (A.program_counter != B.program_counter)
		return 2;
	if (A.accumulator != B.accumulator)
		return 3;
	if (A.X != B.X)
		return 4;
	if (A.Y != B.Y)
		return 5;
	if (A.emulation_mode != B.emulation_mode)
		return 6;
	if (A.p_register != B.p_register)
		return 7;

	return 0;

}