#pragma once

#include <Python.h>
#include <frameobject.h>
#include "vmprof_common.h"

int vmp_walk_and_record_python_stack(PyFrameObject *frame, void **data,
                                     int max_depth);

void* vmp_get_virtual_ip(char* sp);

int vmp_native_enabled(void);
int vmp_native_sp_offset(void);
void vmp_native_enable(int offset);
