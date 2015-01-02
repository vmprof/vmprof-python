
#include <Python.h>
#include "hotpatch/tramp.h"
#include "hotpatch/bin_api.h"
#include <frameobject.h>
#include "vmprof.h"

static PyObject* (*Original_PyEval_EvalFrameEx)(PyFrameObject *f, int throwflag);
int initialized = 0;

static void* PyEval_GetVirtualIp(PyFrameObject* f) {
    char buf[4096];
    char* co_name = PyString_AsString(f->f_code->co_name);
    // set the first bit to 1; on my system, such address space is unused, so
    // we don't risk spurious conflicts with loaded libraries. The proper
    // solution would be to tell the linker to reserve us some address space
    // and use that.
    unsigned long res = (unsigned long)f->f_code | 0x8000000000000000;
    snprintf(buf, 4096, "py:%s", co_name);
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
	insert_tramp("PyEval_EvalFrameEx", &cpyprof_PyEval_EvalFrameEx);
} 

PyObject *enable_vmprof(PyObject* self, PyObject *args)
{
	char *name;

	if (!initialized) {
		init_cpyprof();
		initialized = 1;
	}
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;
	vmprof_enable(name, -1);
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *disable_vmprof(PyObject* self, PyObject *args)
{
	vmprof_disable();
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

PyMODINIT_FUNC init_vmprof(void)
{
	PyObject *m;

    m = Py_InitModule("_vmprof", VmprofMethods);
    if (m == NULL)
        return;
}
