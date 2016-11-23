/*[clinic input]
module _vmprof
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=b443489e38f2be7d]*/

#define _GNU_SOURCE 1

#include <Python.h>
#include <frameobject.h>
#include <signal.h>
#include "clinic/_vmprof.c.h"


#include "_vmprof.h"

static volatile int is_enabled = 0;

#if defined(__unix__) || defined(__APPLE__)
#include "vmprof_main.h"
#include "hotpatch/tramp.h"
#include "hotpatch/bin_api.h"
#else
#include "vmprof_main_win32.h"
#endif

static destructor Original_code_dealloc = 0;
static PyObject *(*Original_PyEval_EvalFrameEx)(PyFrameObject *f,
                                                int throwflag) = 0;
static ptrdiff_t mainloop_sp_offset;


static int emit_code_object(PyCodeObject *co)
{
    char buf[MAX_FUNC_NAME + 1];
    char *co_name, *co_filename;
    int co_firstlineno;
    int sz;
#if PY_MAJOR_VERSION >= 3
    co_name = PyUnicode_AsUTF8(co->co_name);
    if (co_name == NULL)
        return -1;
    co_filename = PyUnicode_AsUTF8(co->co_filename);
    if (co_filename == NULL)
        return -1;
#else
    co_name = PyString_AS_STRING(co->co_name);
    co_filename = PyString_AS_STRING(co->co_filename);
#endif
    co_firstlineno = co->co_firstlineno;

    sz = snprintf(buf, MAX_FUNC_NAME / 2, "py:%s", co_name);
    if (sz < 0) sz = 0;
    if (sz > MAX_FUNC_NAME / 2) sz = MAX_FUNC_NAME / 2;
    snprintf(buf + sz, MAX_FUNC_NAME / 2, ":%d:%s", co_firstlineno,
             co_filename);
    return vmprof_register_virtual_function(buf, CODE_ADDR_TO_UID(co), 500000);
}

static int _look_for_code_object(PyObject *o, void *all_codes)
{
    if (PyCode_Check(o) && !PySet_Contains((PyObject *)all_codes, o)) {
        Py_ssize_t i;
        PyCodeObject *co = (PyCodeObject *)o;
        if (emit_code_object(co) < 0)
            return -1;
        if (PySet_Add((PyObject *)all_codes, o) < 0)
            return -1;

        /* as a special case, recursively look for and add code
           objects found in the co_consts.  The problem is that code
           objects are not created as GC-aware in CPython, so we need
           to hack like this to hope to find most of them. 
        */
        i = PyTuple_Size(co->co_consts);
        while (i > 0) {
            --i;
            if (_look_for_code_object(PyTuple_GET_ITEM(co->co_consts, i),
                                      all_codes) < 0)
                return -1;
        }
    }
    return 0;
}

static void emit_all_code_objects(void)
{
    PyObject *gc_module = NULL, *lst = NULL, *all_codes = NULL;
    Py_ssize_t i, size;

    gc_module = PyImport_ImportModuleNoBlock("gc");
    if (gc_module == NULL)
        goto error;

    lst = PyObject_CallMethod(gc_module, "get_objects", "");
    if (lst == NULL || !PyList_Check(lst))
        goto error;

    all_codes = PySet_New(NULL);
    if (all_codes == NULL)
        goto error;

    size = PyList_GET_SIZE(lst);
    for (i = 0; i < size; i++) {
        PyObject *o = PyList_GET_ITEM(lst, i);
        if (o->ob_type->tp_traverse &&
            o->ob_type->tp_traverse(o, _look_for_code_object, (void *)all_codes)
                < 0)
            goto error;
    }

 error:
    Py_XDECREF(all_codes);
    Py_XDECREF(lst);
    Py_XDECREF(gc_module);
}

static void cpyprof_code_dealloc(PyObject *co)
{
    if (is_enabled) {
        emit_code_object((PyCodeObject *)co);
        /* xxx error return values are ignored */
    }
    Original_code_dealloc(co);
}

static void init_cpyprof(void)
{
#if CPYTHON_HAS_FRAME_EVALUATION
    PyThreadState *tstate = PyThreadState_GET();
    tstate->interp->eval_frame = cpython_vmprof_PyEval_EvalFrameEx;
#else
    if (Original_PyEval_EvalFrameEx == 0) {
        Original_PyEval_EvalFrameEx = PyEval_EvalFrameEx;
        // monkey-patch PyEval_EvalFrameEx
        init_memprof_config_base();
        bin_init();
        create_tramp_table();
        size_t tramp_size;
        tramp_start = insert_tramp("PyEval_EvalFrameEx",
                                   &cpyprof_PyEval_EvalFrameEx,
                                   &tramp_size);
        tramp_end = tramp_start + tramp_size;
    }
#endif
    vmp_native_enable(0);
}


#if CPYTHON_HAS_FRAME_EVALUATION
__attribute__((optimize("O3")))
PyObject* cpython_vmprof_PyEval_EvalFrameEx(PyFrameObject *f, int throwflag)
{
    // move the frame to a caller saved register. e.g. rbx!
    // the previous method from did not really work for me
    // (it did not find the correct value in the stack slot)
    register PyFrameObject * rbx asm("rbx");
    asm volatile("mov %%rdi, %0\n\t"
                 "call %1\n\t"
                : : "r" (rbx), "r" (_PyEval_EvalFrameDefault));
}
#endif

//__attribute__((optimize("O2")))
//static PyObject* cpyprof_PyEval_EvalFrameEx(PyFrameObject *f, int throwflag)
//{
//    register void* _rsp asm("rsp");
//    volatile PyFrameObject *f2 = f;    /* this prevents the call below from
//                                          turning into a tail call */
//    // TODO
//    //if (!mainloop_get_virtual_ip) {
//    //    mainloop_sp_offset = (char*)&f2 - (char*)_rsp;
//    //    mainloop_get_virtual_ip = &get_virtual_ip;
//    //}
//    return Original_PyEval_EvalFrameEx(f, throwflag);
//}

/*[clinic input]
_vmprof.enable
    fd: 'i'
    interval: 'd'
    memory: 'i' = 0
    lines: 'i' = 0

Enable profiling
[clinic start generated code]*/

static PyObject *
_vmprof_enable_profiling_impl(PyObject *module, int fd, double interval,
                              int memory, int lines)
/*[clinic end generated code: output=49fa3c6bd54543fe input=06db7fb1ae0b1c6c]*/
{
    char *p_error;

    assert(fd >= 0 && "file descripter provided to vmprof must not" \
                      " be less then zero.");

    if (is_enabled) {
        PyErr_SetString(PyExc_ValueError, "vmprof is already enabled");
        return NULL;
    }

    init_cpyprof();

    if (!Original_code_dealloc) {
        Original_code_dealloc = PyCode_Type.tp_dealloc;
        PyCode_Type.tp_dealloc = &cpyprof_code_dealloc;
    }

    p_error = vmprof_init(fd, interval, memory, lines, "cpython");
    if (p_error) {
        PyErr_SetString(PyExc_ValueError, p_error);
        return NULL;
    }

    if (vmprof_enable(memory) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    is_enabled = 1;
    profile_lines = lines;

    Py_INCREF(Py_None);
    return Py_None;
}

/*[clinic input]
_vmprof.disable

Disable profiling
[clinic start generated code]*/

static PyObject *
_vmprof_disable_vmprof_impl(PyObject *module)
/*[clinic end generated code: output=c65823d040269360 input=3fa7a69e8df4cb04]*/
{
    if (!is_enabled) {
        PyErr_SetString(PyExc_ValueError, "vmprof is not enabled");
        return NULL;
    }
    is_enabled = 0;
    vmprof_ignore_signals(1);
    emit_all_code_objects();
    if (vmprof_disable() < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    if (PyErr_Occurred())
        return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

/*[clinic input]
_vmprof.write_all_code_objects

Write eagerly all the IDs of code objects
[clinic start generated code]*/

static PyObject *
_vmprof_write_all_code_objects_impl(PyObject *module)
/*[clinic end generated code: output=607a30e701adee35 input=3a6826a3999ef5cd]*/
{
    if (!is_enabled) {
        PyErr_SetString(PyExc_ValueError, "vmprof is not enabled");
        return NULL;
    }
    emit_all_code_objects();
    if (PyErr_Occurred())
        return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}


/*[clinic input]
_vmprof.sample_stack_now

Sample the current stack trace of the Python process.
[clinic start generated code]*/

static PyObject *
_vmprof_sample_stack_now_impl(PyObject *module)
/*[clinic end generated code: output=1aadffe94bffdbd0 input=7885cdde43892ddb]*/
{
    PyThreadState * tstate = NULL;
    PyObject * list;
    list = PyList_New(0);
    if (list == NULL) {
        goto error;
    }

    tstate = PyGILState_GetThisThreadState();
    void ** m = (void**)malloc(SINGLE_BUF_SIZE);
    if (m == NULL) {
        PyErr_SetString(PyExc_MemoryError, "could not allocate buffer for stack trace");
        return NULL;
    }
    int entry_count = get_stack_trace(tstate, m, MAX_STACK_DEPTH-1, 1);

    for (int i = 0; i < entry_count; i++) {
        void * routine_ip = m[i];
        if (ROUTINE_IS_PYTHON(routine_ip)) {
            PyCodeObject * code = (PyCodeObject*)routine_ip;
            PyObject * name = code->co_name;
            PyList_Append(list, name);
        } else {
            // a native routine!
            const char * name = vmp_get_symbol_for_ip(routine_ip);
            PyObject * str = PyStr_NEW(name == NULL ? "unknown symbol" : name);
            if (str == NULL) {
                goto error;
            }
            PyList_Append(list, str);
        }
    }

    free(m);

    Py_INCREF(list);
    return list;
error:
    Py_DECREF(list);
    Py_INCREF(Py_None);
    return Py_None;
}

/*[clinic input]
_vmprof.testing_enable

Setup the library specifically for testing.
[clinic start generated code]*/

static PyObject *
_vmprof_testing_enable_impl(PyObject *module)
/*[clinic end generated code: output=c4b9ed8350f30977 input=edb73a6d4c7b530c]*/
{
    init_cpyprof();
    is_enabled = 1;
    Py_INCREF(Py_None);
    return Py_None;
}

/*[clinic input]
_vmprof.testing_disable

Tear down the library after testing has been completed
[clinic start generated code]*/

static PyObject *
_vmprof_testing_disable_impl(PyObject *module)
/*[clinic end generated code: output=0b39c2ee6280b9b9 input=9ec04784a572f5a6]*/
{
    Py_INCREF(Py_None);
    return Py_None;
}


static PyMethodDef VMProfMethods[] = {
    _VMPROF_ENABLE_METHODDEF
    _VMPROF_DISABLE_METHODDEF
    _VMPROF_WRITE_ALL_CODE_OBJECTS_METHODDEF
    _VMPROF_SAMPLE_STACK_NOW_METHODDEF
    _VMPROF_TESTING_ENABLE_METHODDEF
    _VMPROF_TESTING_DISABLE_METHODDEF
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef VmprofModule = {
    PyModuleDef_HEAD_INIT,
    "_vmprof",
    "",  // doc
    -1,  // size
    VMProfMethods
};

PyMODINIT_FUNC PyInit__vmprof(void)
{
    return PyModule_Create(&VmprofModule);
}
#else
PyMODINIT_FUNC init_vmprof(void)
{
    Py_InitModule("_vmprof", VMProfMethods);
}
#endif
