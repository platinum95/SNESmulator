#include "cartridge.h"
#include "system.h"

int main() {

    const char* romPath = "smk.sfc";
    if ( cartridgeLoadRom( romPath ) ) {
        return 1;
    }
    
    if ( startup() ){
        return 1;
    }

    begin_execution();

    return 0;
}