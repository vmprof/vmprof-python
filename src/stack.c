#include "stack.h"

#include <libunwind.h>

static int vmp_native_traces_enabled = 0;
static int vmp_native_traces_sp_offset = -1;

int vmp_walk_and_record_python_stack(PyFrameObject *frame, void ** result,
                                     int max_depth)
{
    void *ip, *sp;
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_proc_info_t pip;

    int ret = unw_init_local(&cursor, &uc);
    if (ret < 0) {
        // could not initialize lib unwind cursor and context
        return -1;
    }

    PyFrameObject * top_most_frame = frame;
    PyFrameObject * compare_frame;
    int depth = 0;
    while (depth < max_depth) {
        if (!vmp_native_traces_enabled) {
            if (top_most_frame == NULL) {
                break;
            }
            // TODO add line profiling
            sp = (void*)CODE_ADDR_TO_UID(top_most_frame->f_code);
            result[depth++] = sp;
            top_most_frame = top_most_frame->f_back;
            continue;
        }
        unw_get_proc_info(&cursor, &pip);

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)&sp) < 0) {
            // could not retrieve
            break;
        }

        if ((void*)pip.start_ip == PyEval_EvalFrameEx) {
            // yes we found one stack entry of the python frames!
            compare_frame = vmp_get_virtual_ip(sp);
            if (compare_frame != top_most_frame) {
                // uh we are screwed! the ip indicates we are have context
                // to a PyEval_EvalFrameEx function, but when we tried to retrieve
                // the stack located py frame it has a different address than the
                // current top_most_frame
                result[depth++] = (void*)-1;
                break;
            }
            sp = (void*)CODE_ADDR_TO_UID(top_most_frame->f_code);
            result[depth++] = sp;
            top_most_frame = top_most_frame->f_back;
#if CPYTHON_HAS_FRAME_EVALUATION
        } else if ((void*)pip.start_ip == _PyEval_EvalFrameDefault) {
            // pass here
#endif
        } else {
            ip = (void*)pip.start_ip;
            // mark native routines with the first bit set,
            // this is possible because compiler align to 8 bytes.
            // TODO need to check if this is possible on other 
            // compiler than e.g. gcc/clang too?
            result[depth++] = (void*)((size_t)ip | 0x1);
        }

        if (unw_step(&cursor) <= 0) {
            break;
        }
    }

    return depth;
}

void* vmp_get_virtual_ip(char* sp) {
    PyFrameObject *f = *(PyFrameObject **)(sp + vmp_native_sp_offset());
    return (void *)CODE_ADDR_TO_UID(f->f_code);
}

int vmp_native_enabled(void) {
    return vmp_native_traces_enabled;
}

int vmp_native_sp_offset(void) {
    return vmp_native_traces_sp_offset;
}

void vmp_native_enable(int offset) {
    vmp_native_traces_enabled = 1;
    vmp_native_traces_sp_offset = 1;
}
