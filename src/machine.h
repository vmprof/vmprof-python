#pragma once

#include "libudis86/udis86.h"

int vmp_machine_bits(void);

const char * vmp_machine_os_name(void);

unsigned int vmp_machine_code_instr_length(char* pc, struct ud * u);
