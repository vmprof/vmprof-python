#pragma once

/**
 * Extract all the known symbols from the current process and
 * log them to the file descriptor. To read them see binary.py funcs:
 *
 * # encoded as a mapping
 * addr = read_word(fd); name = read_string(fd)
 *
 * A) It is not allowed to have two addresses (virtual ones only valid
 * in the curent process) in this mapping to point to several symbols.
 * B) No duplicates are logged
 */
void dump_all_known_symbols(int fd);
