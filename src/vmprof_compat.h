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

#if !(defined(__unix__) || defined(__APPLE__))
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 
#endif

/**
 * Write the time and zone now.
 */
int _write_time_now(int marker) {
    struct timezone_buf {
        int64_t tv_sec;
        int64_t tv_usec;
    };
    int size = 1+sizeof(struct timezone_buf)+8;
    char buffer[size];
    struct timezone_buf buf;
    (void)memset(&buffer, 0, size);

    assert((marker == MARKER_TRAILER || marker == MARKER_TIME_N_ZONE) && \
           "marker must be either a trailer or time_n_zone!");

#if defined(__unix__) || defined(__APPLE__)
    struct timeval tv;
    time_t now;
    struct tm tm;


    /* copy over to the struct */
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    if (time(&now) == (time_t)-1) {
        return -1;
    }
    if (localtime_r(&now, &tm) == NULL) {
        return -1;
    }
    buf.tv_sec = tv.tv_sec;
    buf.tv_usec = tv.tv_usec;
    strncpy(((char*)buffer)+size-8, tm.tm_zone, 8);
#else
    /**
     * http://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows
     */

    // Note: some broken versions only have 8 trailing zero's, the correct
    // epoch has 9 trailing zero's
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    buf.tv_sec = ((time - EPOCH) / 10000000L);
    buf.tv_usec = (system_time.wMilliseconds * 1000);

    // time zone not implemented on windows
    memset(((char*)buffer)+size-8, 0, 8);
#endif

    buffer[0] = marker;
    (void)memcpy(buffer+1, &buf, sizeof(struct timezone_buf));
    _write_all(buffer, size);
    return 0;
}
