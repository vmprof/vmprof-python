#pragma once

#include <Python.h>

#if PY_MAJOR_VERSION >= 3
    #define PyStr_AS_STRING PyBytes_AS_STRING
    #define PyStr_GET_SIZE PyBytes_GET_SIZE
    #define PyStr_NEW      PyUnicode_FromString
#else
    #define PyStr_AS_STRING PyString_AS_STRING
    #define PyStr_GET_SIZE PyString_GET_SIZE
    #define PyStr_NEW      PyString_FromString
#endif
