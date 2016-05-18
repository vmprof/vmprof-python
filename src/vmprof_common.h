#include <stddef.h>
#include <zlib.h>

#define MAX_FUNC_NAME 1024

static gzFile profile_file = NULL;
static long prepare_interval_usec = 0;
static long profile_interval_usec = 0;
static int opened_profile(char *interp_name, int memory);

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
char *vmprof_init(int fd, double interval, int memory, char *interp_name)
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
        return "memory tracking not supported on non-linux";
#endif
    assert(fd >= 0);
    profile_file = gzdopen(dup(fd), "ab3");
    if (opened_profile(interp_name, memory) < 0) {
        profile_file = NULL;
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
#if PY_MAJOR_VERSION >= 3
    return (PyThreadState*)_Py_atomic_load_relaxed(&_PyThreadState_Current);
#else
    return _PyThreadState_Current;
#endif
}
