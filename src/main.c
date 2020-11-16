#include "cartridge.h"
#include "system.h"

int main( char argc, char **argv ) {

    const char* romPath = "smk.sfc";
    if ( LoadRom( romPath ) ) {
        return 1;
    }
    
    if ( startup() ){
        return 1;
    }

    begin_execution();

    return 0;
}