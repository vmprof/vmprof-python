#include <Python.h>

#if PY_MAJOR_VERSION >= 3
    #define PyStr_AS_STRING PyBytes_AS_STRING
#else
    #define PyStr_AS_STRING PyString_AS_STRING
#endif

#if PY_MAJOR_VERSION >= 3
    #define PyStr_GET_SIZE PyBytes_GET_SIZE
#else
    #define PyStr_GET_SIZE PyString_GET_SIZE
#endif
