
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#define HEX_LOC 1




char* getLine(FILE * file) {
	int n = 50;
	char* buf = malloc(n);
	fgets(buf, n, file);
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



FILE *comp_file, *out_file;
int start_comp() {
	comp_file = fopen("funcs.txt", "r");
	out_file = fopen("output.txt", "w");
	return 0;
}

int main() {
	start_comp();
	char list[255][40];
	for(int i = 0; i < 255; i++){
		char nextLine[40];
		char* some = getLine(comp_file);
		strcpy(&nextLine[0], some);
		//printf(nextLine);
		uint8_t hex_code = get8(nextLine, HEX_LOC);
		char toPrint[40] = "spc700_instructions[0x00] = ";
		toPrint[22] = nextLine[1];
		toPrint[23] = nextLine[2];
		strcpy(toPrint + 27, nextLine);
		strcpy(&list[hex_code][0], toPrint);
	}
	for(int i = 0; i < 255; i++){
		fputs(list[i], out_file);
		if((i & 0x0F) == 0x00)
			fputs("\n", out_file);
	}
	return 0;
}