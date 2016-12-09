/**
 * http://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows
 */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 

int vmprof_gettimeofday(long *dst)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    dst[0] = (long) ((time - EPOCH) / 10000000L);
    dst[1] = (long) (system_time.wMilliseconds * 1000);

    return 0;
}

int vmprof_gettimezone(void *dst) {
    // Not implemented
    memset(dst, '\0', 5);
    return 0;
}
