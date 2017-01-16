#pragma once

#include <Python.h>
#include <stddef.h>
#include <sys/time.h>
#include <time.h>
#include "machine.h"

static int _write_all(const char *buf, size_t bufsize);

#include "vmprof_compat.h"
#include "_vmprof.h"
#include "vmprof_mt.h"

#define MAX_FUNC_NAME 1024


#define RPY_EXTERN static
#define VMPROF_ADDR_OF_TRAMPOLINE(x)  ((x) == &cpyprof_PyEval_EvalFrameEx)
#define CPYTHON_GET_CUSTOM_OFFSET
static void *tramp_start, *tramp_end;

static int profile_file = -1;
static long prepare_interval_usec = 0;
static long profile_interval_usec = 0;
static int profile_lines = 0;

static int opened_profile(const char *interp_name, int memory, int lines, int native);

#if defined(__unix__) || defined(__APPLE__)
static struct profbuf_s *volatile current_codes;
#endif

#define MAX_STACK_DEPTH   \
    ((SINGLE_BUF_SIZE - sizeof(struct prof_stacktrace_s)) / sizeof(void *))

static int _write_meta(const char * key, const char * value)
{
    char marker = MARKER_META;
    long x = strlen(key);
    _write_all(&marker, 1);
    _write_all((char*)&x, sizeof(long));
    _write_all(key, x);
    x = strlen(value);
    _write_all((char*)&x, sizeof(long));
    _write_all(value, x);
    return 0;
}


typedef struct prof_stacktrace_s {
    char padding[sizeof(long) - 1];
    char marker;
    long count, depth;
    void *stack[];
} prof_stacktrace_s;

RPY_EXTERN
char *vmprof_init(int fd, double interval, int memory, int lines, const char *interp_name, int native)
{
    if (interval < 1e-6 || interval >= 1.0)
        return "bad value for 'interval'";
    prepare_interval_usec = (int)(interval * 1000000.0);

    if (prepare_concurrent_bufs() < 0)
        return "out of memory";
#if defined(__unix__) || defined(__APPLE__)
    current_codes = NULL;
#else
    if (memory) {
        return "memory tracking only supported on unix";
    }
    if (native) {
        return "native profiling only supported on unix";
    }
#endif
    assert(fd >= 0);
    profile_file = fd;
    if (opened_profile(interp_name, memory, lines, native) < 0) {
        profile_file = -1;
        return strerror(errno);
    }
    return NULL;
}

static int read_trace_from_cpy_frame(PyFrameObject *frame, void **result, int max_depth)
{
    int depth = 0;

    while (frame && depth < max_depth) {
        if (profile_lines) {
            // In the line profiling mode we save a line number for every frame.
            // Actual line number isn't stored in the frame directly (f_lineno points to the
            // beginning of the frame), so we need to compute it from f_lasti and f_code->co_lnotab.
            // Here is explained what co_lnotab is:
            //    https://svn.python.org/projects/python/trunk/Objects/lnotab_notes.txt

            // NOTE: the profiling overhead can be reduced by storing co_lnotab in the dump and
            // moving this computation to the reader instead of doing it here.
            char *lnotab = PyStr_AS_STRING(frame->f_code->co_lnotab);

            if (lnotab != NULL) {
                long line = (long)frame->f_lineno;
                int addr = 0;

                int len = PyStr_GET_SIZE(frame->f_code->co_lnotab);

                int j;
                for (j = 0; j<len; j+=2) {
                    addr += lnotab[j];
                    if (addr>frame->f_lasti) {
                        break;
                    }
                    line += lnotab[j+1];
                }
                result[depth++] = (void*) line;
            } else {
                result[depth++] = (void*) 0;
            }
        }

        result[depth++] = (void*)CODE_ADDR_TO_UID(frame->f_code);
        frame = frame->f_back;
    }
    return depth;
}

static int opened_profile(const char *interp_name, int memory, int lines, int native)
{
    int success;
    struct {
        long hdr[5];
        char interp_name[259];
    } header;

    size_t namelen = strnlen(interp_name, 255);

    header.hdr[0] = 0;
    header.hdr[1] = 3;
    header.hdr[2] = 0;
    header.hdr[3] = prepare_interval_usec;
    header.hdr[4] = 0;
    header.interp_name[0] = MARKER_HEADER;
    header.interp_name[1] = '\x00';
    header.interp_name[2] = VERSION_TIMESTAMP;
    header.interp_name[3] = memory*PROFILE_MEMORY + lines*PROFILE_LINES + \
                            native*PROFILE_NATIVE;
    header.interp_name[4] = namelen;

    memcpy(&header.interp_name[5], interp_name, namelen);
    success = _write_all((char*)&header, 5 * sizeof(long) + 5 + namelen);
    if (success < 0) {
        return success;
    }

    /* Write the time and the zone to the log file, profiling will start now */
    (void)_write_time_now(MARKER_TIME_N_ZONE);

    /* write some more meta information */
    _write_meta("os", vmp_machine_os_name());
    int bits = vmp_machine_bits();
    if (bits == 64) {
        _write_meta("bits", "64");
    } else if (bits == 32) {
        _write_meta("bits", "32");
    }

    return success;
}


/* Seems that CPython 3.5.1 made our job harder.  Did not find out how
   to do that without these hacks.  We can't use PyThreadState_GET(),
   because that calls PyThreadState_Get() which fails an assert if the
   result is NULL. */
#if PY_MAJOR_VERSION >= 3 && !defined(_Py_atomic_load_relaxed)
                             /* this was abruptly un-defined in 3.5.1 */
void *volatile _PyThreadState_Current;
   /* XXX simple volatile access is assumed atomic */
#  define _Py_atomic_load_relaxed(pp)  (*(pp))
#endif

