#pragma once

#ifdef NATIVE_FRAMES

#include <libunwind.h>

typedef struct _native_mapping {
    PyFrameObject * frame;
    unw_word_t arch_sp;
    struct _native_mapping * back;
    int depth;
} native_mapping_t;

/**
 * Thread local native mapping. Connects C stack frames with
 * PyFrameObjects
 */
__thread native_mapping_t * _tl_natmap = NULL;

static
int vmprof_trace_func(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
    unw_context_t ctx;
    unw_cursor_t cursor;
    unw_word_t off;

    if (what == PyTrace_CALL || what == PyTrace_C_CALL) {
        unw_getcontext(&ctx);
        unw_init_local(&cursor, &ctx);
        unw_proc_info_t proc_info;

        int level = 0;
        do {
            unw_get_proc_info(&cursor, &proc_info);
            if (proc_info.start_ip == &PyEval_EvalFrameEx) {
                native_mapping_t * m = malloc(sizeof(native_mapping_t));
                m->back = _tl_natmap;
                if (_tl_natmap == NULL) { m->depth = 0; }
                else { m->depth = _tl_natmap; }
                m->frame = frame;
                unw_get_reg(&cursor, UNW_REG_SP, &(m->arch_sp));
                _tl_natmap = m;
                break;
            }
            level += 1;
        } while (unw_step(&cursor) > 0);
    } else if (what == PyTrace_RETURN) {
        if (_tl_natmap != NULL) {
            _tl_natmap = _tl_natmap->back;
        }
    }

    return 0;
}

#endif
