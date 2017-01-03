#pragma once

#include <Python.h>
#include <frameobject.h>
#include "_vmprof.h"

int vmp_walk_and_record_python_stack(PyFrameObject *frame, void **data,
                                     int max_depth);

void* vmp_get_virtual_ip(char* sp);

int vmp_native_enabled(void);
int vmp_native_sp_offset(void);
int vmp_native_enable(int offset);
void vmp_get_symbol_for_ip(void * ip, char * name, int length);
int vmp_ignore_ip(ptr_t ip);
int vmp_binary_search_ranges(ptr_t ip, ptr_t * l, int count);
int vmp_native_symbols_read(void);

int vmp_ignore_symbol_count(void);
ptr_t * vmp_ignore_symbols(void);
void vmp_set_ignore_symbols(ptr_t * symbols, int count);
void vmp_native_disable(void);

#ifdef __unix__
int vmp_read_vmaps(const char * fname);
#endif
