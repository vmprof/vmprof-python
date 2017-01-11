#pragma once

/**
 * Extract all the known symbols from the current process and
 * log them to the file descriptor. To read them see binary.py funcs:
 *
 * # encoded as a mapping
 * addr = read_word(fd); name = read_string(fd)
 */
void dump_all_known_symbols(int fd);
