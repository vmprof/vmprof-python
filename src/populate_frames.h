/* This code was taken from https://github.com/GoogleCloudPlatform/cloud-profiler-python/blob/main/googlecloudprofiler/src/populate_frames.h */ 

#ifndef pp_frames
#define pp_frames

#include <Python.h>

#include <frameobject.h>

// 0x030E0000 is 3.14.
#define PY_314 0x030E0000

#define Py_BUILD_CORE
#if PY_VERSION_HEX >= PY_314
// Python 3.14 moved frame internals to pycore_interpframe.h
#include "internal/pycore_interpframe.h"
#else
#include "internal/pycore_frame.h"
#endif
#undef Py_BUILD_CORE

_PyInterpreterFrame *unsafe_PyThreadState_GetInterpreterFrame(PyThreadState *tstate);

PyCodeObject *unsafe_PyInterpreterFrame_GetCode(_PyInterpreterFrame *frame);

_PyInterpreterFrame *unsafe_PyInterpreterFrame_GetBack(_PyInterpreterFrame *frame);

int _PyInterpreterFrame_GetLine(_PyInterpreterFrame *frame);

#endif
