#include <stddef.h>

#define MAX_FUNC_NAME 1024

static int profile_file = -1;
static long prepare_interval_usec = 0;
static long profile_interval_usec = 0;
static int opened_profile(char *interp_name, int memory);
/** Whether to use ITIMER_REAL instead of ITIMER_PROF */
static int use_wall_time = 0;

#if defined(__unix__) || defined(__APPLE__)
static struct profbuf_s *volatile current_codes;
#endif

#define MAX_STACK_DEPTH   \
    ((SINGLE_BUF_SIZE - sizeof(struct prof_stacktrace_s)) / sizeof(void *))

#define MARKER_STACKTRACE '\x01'
#define MARKER_VIRTUAL_IP '\x02'
#define MARKER_TRAILER '\x03'
#define MARKER_INTERP_NAME '\x04'   /* deprecated */
#define MARKER_HEADER '\x05'

#define VERSION_BASE '\x00'
#define VERSION_THREAD_ID '\x01'
#define VERSION_TAG '\x02'
#define VERSION_MEMORY '\x03'

typedef struct prof_stacktrace_s {
    char padding[sizeof(long) - 1];
    char marker;
    long count, depth;
    void *stack[];
} prof_stacktrace_s;


RPY_EXTERN
char *vmprof_init(int fd, double interval, int memory, int wall_time, char *interp_name)
{
    if (interval < 1e-6 || interval >= 1.0)
        return "bad value for 'interval'";
    prepare_interval_usec = (int)(interval * 1000000.0);

    if (prepare_concurrent_bufs() < 0)
        return "out of memory";
#if defined(__unix__) || defined(__APPLE__)
    current_codes = NULL;
#endif

#if !defined(__unix__)
    if (memory)
        return "memory tracking only supported on linux";
    if (wall_time)
        return "wall time profiling only supported on unix";
#endif
    use_wall_time = wall_time;

    assert(fd >= 0);
    profile_file = fd;
    if (opened_profile(interp_name, memory) < 0) {
        profile_file = -1;
        return strerror(errno);
    }
    return NULL;
}

static int read_trace_from_cpy_frame(PyFrameObject *frame, void **result, int max_depth)
{
    int depth = 0;

    while (frame && depth < max_depth) {
        result[depth++] = (void*)CODE_ADDR_TO_UID(frame->f_code);
        frame = frame->f_back;
    }
    return depth;
}

static int _write_all(const char *buf, size_t bufsize);

static int opened_profile(char *interp_name, int memory)
{
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
    if (memory) {
        header.interp_name[2] = VERSION_MEMORY;
    } else {
        header.interp_name[2] = VERSION_THREAD_ID;
    }
    header.interp_name[3] = namelen;
    memcpy(&header.interp_name[4], interp_name, namelen);
    return _write_all((char*)&header, 5 * sizeof(long) + 4 + namelen);
}

// for whatever reason python-dev decided to hide that one
#if PY_MAJOR_VERSION >= 3 && !defined(_Py_atomic_load_relaxed)
                                 /* this was abruptly un-defined in 3.5.1 */
    extern void *volatile _PyThreadState_Current;
       /* XXX simple volatile access is assumed atomic */
#  define _Py_atomic_load_relaxed(pp)  (*(pp))
#endif
 
PyThreadState* get_current_thread_state(void)
{
    PyThreadState *tstate;
#if PY_MAJOR_VERSION >= 3
    tstate = (PyThreadState*)_Py_atomic_load_relaxed(&_PyThreadState_Current);
#else
    tstate = _PyThreadState_Current;
#endif

    if (tstate == NULL) {
        /* This happens if we use wall time (ITIMER_REAL) and the current
         * thread is waiting for a syscall to finish, in which case
         * _PyThreadState_Current is NULL.  But we can still obtain the thread
         * state by manually looking through the available thread states.
         *
         * In general we find more than one thread state there; these threads
         * are either waiting for a syscall or waiting for the GIL.
         *
         * In the syscall case we should consider them for inclusion in the
         * profile.
         *
         * In the GIL case they aren't actually spending any time
         * (not in any practical definition of "spending time" anyways) and
         * should not be included in the profile.
         *
         * The problem is that we can't distinguish between the two cases.
         * Therefore, if we find more than one thread here, we include none in
         * the profile.  This means that wall time profiles DO NOT WORK with
         * multiple threads. To be specific, a wall time profile of multiple
         * threads behaves just like a normal (non-wall-time) profile.
         *
         * In the future this could be worked around by unwinding the C call
         * stack to look for a call to "take_gil", which is an indicator for
         * the thread waiting for the GIL.
         */
        PyInterpreterState *interp = PyInterpreterState_Head();
        while (interp != NULL) {
            tstate = PyInterpreterState_ThreadHead(interp);
            if (tstate != NULL) {
               if (PyThreadState_Next(tstate) != NULL) {
                    /* Found multiple thread states */
                    tstate = NULL;
                }
                break;
            }
            interp = PyInterpreterState_Next(interp);
        }
    }

    return tstate;
}
