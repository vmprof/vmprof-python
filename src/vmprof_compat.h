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

#if defined(_WIN32)
    #include "time_win32.c"
#else
    #include "time_unix.c"
#endif

#if defined(__unix__)
    #include "rss_unix.h"
#elif defined(__APPLE__)
    #include "rss_darwin.h"
#endif
