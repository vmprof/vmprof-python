#pragma once

/**
 * What is the usual word size of the processor? 64bit? 32bit?
 */
int vmp_machine_bits(void);

/**
 * Return the human readable name of the operating system.
 */
const char * vmp_machine_os_name(void);

/**
 * How many bytes does the x86 instruction take at pc[0..].
 *
 * Returns 0 on failure.
 */
unsigned int vmp_machine_code_instr_length(char* pc);
