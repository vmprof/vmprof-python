
#include <Python.h>
#include "hotpatch/tramp.h"
#include "hotpatch/bin_api.h"
#include <frameobject.h>
#include "vmprof.h"

static PyObject* (*Original_PyEval_EvalFrameEx)(PyFrameObject *f, int throwflag);
int initialized = 0;
PyObject* vmprof_mod = NULL;

#define UNUSED_FLAG 0x1000 // this is a flag unused since Python2.5, let's hope
// noone uses it

static void* PyEval_GetVirtualIp(PyFrameObject* f, long *thread_id) {
    char buf[4096];
    char *co_name, *co_filename;
    int co_firstlineno;
    unsigned long code_id = (unsigned long)f->f_code | 0x7000000000000000;

    /* XXX We emit the "py:" string identifying this code object the
       first time it is seen, as identified by the absence of the
       UNUSED_FLAG.  If we disable() and later re-enable() vmprof, the
       code objects already produced won't be produced again; that's
       the reason for the workaround '_virtual_ips_so_far' in the pure
       Python vmprof.disable() function.

       In case the code object is freed and another one allocated at
       the same address, this logic will reuse the same code_id, but
       produce a different "py:" string identifier.  The code that
       later loads the profiler file should (ideally) detect that
       there are multiple ids for the same address and, at least,
       adjust what it prints for that address.

       We need to modify PyCode_Type.tp_hash to ignore the UNUSED_FLAG
       in co_flags.  Careful, in a multithreaded process the UNUSED_FLAG
       may be added concurrently.

       To support 32-bit, this should be modified to no longer do the
       " | 0x7000000000000000" part.  The loader of the file can know
       if an id corresponds to a "real" assembler address or a
       "virtual" id produced here: it can do so by checking if there
       is a "py:" string identifier corresponding to the id or not.

       Other ideas have been considered (see #pypy irc log of August
       17, 2015) but it seems that the current one has the least
       amount of drawbacks: there is only the reuse of code_id's for
       short-lived code objects.
    */
#if PY_MAJOR_VERSION < 3
    *thread_id = f->f_tstate->thread_id;
#endif
    if (f->f_code->co_flags & UNUSED_FLAG)
        return (void*)res;
    f->f_code->co_flags |= UNUSED_FLAG;
#if PY_MAJOR_VERSION >= 3
    co_name = PyUnicode_AsUTF8(f->f_code->co_name);
    co_filename = PyUnicode_AsUTF8(f->f_code->co_filename);
#else
    co_name = PyString_AsString(f->f_code->co_name);
    co_filename = PyString_AsString(f->f_code->co_filename);
#endif
    co_firstlineno = f->f_code->co_firstlineno;
    snprintf(buf, 4096, "py:%s:%d:%s", co_name, co_firstlineno, co_filename);
    vmprof_register_virtual_function(buf, (void*)res, NULL);
    return (void*)res;
}

__attribute__((optimize("O2")))
PyObject* cpyprof_PyEval_EvalFrameEx(PyFrameObject *f, int throwflag) {
    register void* _rsp asm("rsp");
    ptrdiff_t offset;
    volatile PyFrameObject *f2 = f;
    offset = (char*)&f2 - (char*)_rsp;
    if (!vmprof_mainloop_func)
        vmprof_set_mainloop(&cpyprof_PyEval_EvalFrameEx, offset, (void*)PyEval_GetVirtualIp);
	return Original_PyEval_EvalFrameEx(f, throwflag);
}

void init_cpyprof(void) {
	Original_PyEval_EvalFrameEx = PyEval_EvalFrameEx;	
	// monkey-patch PyEval_EvalFrameEx
	init_memprof_config_base();
	bin_init();
	create_tramp_table();
	size_t tramp_size;
	void* tramp_addr = insert_tramp("PyEval_EvalFrameEx", &cpyprof_PyEval_EvalFrameEx, &tramp_size);
	vmprof_set_tramp_range(tramp_addr, tramp_addr+tramp_size);
}

PyObject *enable_vmprof(PyObject* self, PyObject *args)
{
	int fd, period_usec;
    double period_float = 0.01;
	char *x = NULL;
    char *buf;
	int x_len = 0;

	if (!initialized) {
		init_cpyprof();
		initialized = 1;
	}
	if (!PyArg_ParseTuple(args, "id|s#", &fd, &period_float, &x, &x_len))
		return NULL;
    buf = (char*)malloc(x_len + sizeof(long) + 4 + 7);
    buf[0] = MARKER_HEADER;
    buf[1] = '\x00';
    buf[2] = VERSION_THREAD_ID;
    buf[3] = '\x07';
    memcpy(buf + 3 + 1, "cpython", 7);
    if (x)
        memcpy(buf + 3 + 8, x, x_len);
    if (period_float < 0 || period_float >= 1) {
        PyErr_Format(PyExc_ValueError, "Period too large or negative");
        return NULL;
    }
    period_usec = (int)(period_float * 1e6 + 0.5);
	if (vmprof_enable(fd, period_usec, 1, buf, x_len + 8 + 3) == -1) {
		PyErr_SetFromErrno(PyExc_OSError);
		return NULL;
	}
    free(buf);
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *disable_vmprof(PyObject* self, PyObject *args)
{
	if (vmprof_disable() == -1) {
		PyErr_SetFromErrno(PyExc_OSError);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef VmprofMethods[] = {
    {"enable",  enable_vmprof, METH_VARARGS,
     "Enable profiling."},
    {"disable", disable_vmprof, METH_VARARGS,
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
    vmprof_mod = Py_InitModule("_vmprof", VmprofMethods);
    if (vmprof_mod == NULL)
        return;	
}
#endif
