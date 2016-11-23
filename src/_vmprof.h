#pragma once

/**
 * This whole setup is very strange. There was just one C file called
 * _vmprof.c which included all *.h files to copy code. Unsure what
 * the goal was with this design, but I assume it just 'GREW'
 *
 * Thus I'm (plan_rich) slowly trying to separate this. *.h files
 * should not have complex implementations (all of them currently have them)
 */

#define SINGLE_BUF_SIZE (8192 - 2 * sizeof(unsigned int))

#define ROUTINE_IS_PYTHON(RIP) ((unsigned long long)RIP & 0x1) == 0
#define ROUTINE_IS_C(RIP) ((unsigned long long)RIP & 0x1) == 1
