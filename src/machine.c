#include "machine.h"

#include <stdio.h>
#include <inttypes.h>

int vmp_machine_bits(void)
{
    return sizeof(void*)*8;
}

const char * vmp_machine_os_name(void)
{
#ifdef _WIN32
   #ifdef _WIN64
      return "win64";
   #endif
  return "win32";
#elif __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_MAC
        return "macos";
    #endif
#elif __linux__
    return "linux";
#else
    #error "Unknown compiler"
#endif
}

#include "libudis86/udis86.h"

unsigned int vmp_machine_code_instr_length(char* pc)
{
    struct ud u;
    ud_init(&u);
    ud_set_input_buffer(&u, (uint8_t*)pc, 12);
    ud_set_mode(&u, 64); // TODO
    return ud_decode(&u);
}
