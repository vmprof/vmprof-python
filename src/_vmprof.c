#define _GNU_SOURCE 1

#include <Python.h>
#include <frameobject.h>

#define RPY_EXTERN static
static PyObject* cpyprof_PyEval_EvalFrameEx(PyFrameObject *, int);
#define VMPROF_ADDR_OF_TRAMPOLINE(x)  ((x) == &cpyprof_PyEval_EvalFrameEx)
#define CPYTHON_GET_CUSTOM_OFFSET

#include "vmprof_main.h"
#include "hotpatch/tramp.h"
#include "hotpatch/bin_api.h"


static destructor Original_code_dealloc = 0;
static PyObject *(*Original_PyEval_EvalFrameEx)(PyFrameObject *f,
                                                int throwflag) = 0;
static ptrdiff_t mainloop_sp_offset;
static int is_enabled = 0;


#define CODE_ADDR_TO_UID(co)  (((unsigned long)(co)) | 0x7000000000000000UL)

static void* get_virtual_ip(char* sp)
{
    /* This returns the address of the code object plus 0x7000000000000000
       as the identifier.  The mapping from identifiers to string
       representations of the code object is done elsewhere, namely:

       * If the code object dies while vmprof is enabled,
         PyCode_Type.tp_dealloc will emit it.  (We don't handle nicely
         for now the case where several code objects are created and die
         at the same memory address.)

       * When _vmprof.disable() is called, then we look around the
         process for code objects and emit all the ones that we can
         find (which we hope is very close to 100% of them).
    */
    PyFrameObject *f = *(PyFrameObject **)(sp + mainloop_sp_offset);
    return (void *)CODE_ADDR_TO_UID(f->f_code);
}

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
        if (emit_code_object((PyCodeObject *)o) < 0)
            return -1;
        if (PySet_Add((PyObject *)all_codes, o) < 0)
            return -1;
    }
    return 0;
}

static void emit_all_code_objects(void)
{
    PyObject *gc_module = NULL, *lst = NULL, *all_codes = NULL;

    gc_module = PyImport_ImportModuleNoBlock("gc");
    if (gc_module == NULL)
        goto error;

    lst = PyObject_CallMethod(gc_module, "get_objects", "");
    if (lst == NULL || !PyList_Check(lst))
        goto error;

    all_codes = PySet_New(NULL);
    if (all_codes == NULL)
        goto error;

    Py_ssize_t i, size = PyList_GET_SIZE(lst);
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

__attribute__((optimize("O2")))
static PyObject* cpyprof_PyEval_EvalFrameEx(PyFrameObject *f, int throwflag)
{
    register void* _rsp asm("rsp");
    volatile PyFrameObject *f2 = f;    /* this prevents the call below from
                                          turning into a tail call */
    if (!mainloop_get_virtual_ip) {
        mainloop_sp_offset = (char*)&f2 - (char*)_rsp;
        mainloop_get_virtual_ip = &get_virtual_ip;
    }
    return Original_PyEval_EvalFrameEx(f, throwflag);
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
    if (!Original_PyEval_EvalFrameEx) {
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
    if (!Original_code_dealloc) {
        Original_code_dealloc = PyCode_Type.tp_dealloc;
        PyCode_Type.tp_dealloc = &cpyprof_code_dealloc;
    }
}

static PyObject *enable_vmprof(PyObject* self, PyObject *args)
{
    int fd, period_usec;
    double interval;
    char *p_error;

    if (!PyArg_ParseTuple(args, "id", &fd, &interval))
        return NULL;
    assert(fd >= 0);

    if (is_enabled) {
        PyErr_SetString(PyExc_ValueError, "vmprof is already enabled");
        return NULL;
    }

    init_cpyprof();
    p_error = vmprof_init(fd, interval, "cpython");
    if (p_error) {
        PyErr_SetString(PyExc_ValueError, p_error);
        return NULL;
    }

    if (vmprof_enable() < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    is_enabled = 1;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *disable_vmprof(PyObject* self, PyObject *noarg)
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

static PyMethodDef VmprofMethods[] = {
    {"enable",  enable_vmprof, METH_VARARGS,
     "Enable profiling."},
    {"disable", disable_vmprof, METH_NOARGS,
     "Disable profiling."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef VmprofModule = {
    PyModuleDef_HEAD_INIT,
    "_vmprof",
    "",  // doc
    -1,  // size
    VmprofMethods
};

PyMODINIT_FUNC PyInit__vmprof(void)
{
    return PyModule_Create(&VmprofModule);
}
#else
PyMODINIT_FUNC init_vmprof(void)
{
    Py_InitModule("_vmprof", VmprofMethods);
}
#endif
