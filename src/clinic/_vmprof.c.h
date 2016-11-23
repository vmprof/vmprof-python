/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_vmprof_enable_profiling__doc__,
"enable_profiling($module, /, fd, interval, memory=0, lines=0)\n"
"--\n"
"\n"
"Enable profiling");

#define _VMPROF_ENABLE_PROFILING_METHODDEF    \
    {"enable_profiling", (PyCFunction)_vmprof_enable_profiling, METH_FASTCALL, _vmprof_enable_profiling__doc__},

static PyObject *
_vmprof_enable_profiling_impl(PyObject *module, int fd, double interval,
                              int memory, int lines);

static PyObject *
_vmprof_enable_profiling(PyObject *module, PyObject **args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"fd", "interval", "memory", "lines", NULL};
    static _PyArg_Parser _parser = {"id|ii:enable_profiling", _keywords, 0};
    int fd;
    double interval;
    int memory = 0;
    int lines = 0;

    if (!_PyArg_ParseStack(args, nargs, kwnames, &_parser,
        &fd, &interval, &memory, &lines)) {
        goto exit;
    }
    return_value = _vmprof_enable_profiling_impl(module, fd, interval, memory, lines);

exit:
    return return_value;
}

PyDoc_STRVAR(_vmprof_disable_vmprof__doc__,
"disable_vmprof($module, /)\n"
"--\n"
"\n"
"Disable profiling");

#define _VMPROF_DISABLE_VMPROF_METHODDEF    \
    {"disable_vmprof", (PyCFunction)_vmprof_disable_vmprof, METH_NOARGS, _vmprof_disable_vmprof__doc__},

static PyObject *
_vmprof_disable_vmprof_impl(PyObject *module);

static PyObject *
_vmprof_disable_vmprof(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return _vmprof_disable_vmprof_impl(module);
}

PyDoc_STRVAR(_vmprof_write_all_code_objects__doc__,
"write_all_code_objects($module, /)\n"
"--\n"
"\n"
"Write eagerly all the IDs of code objects");

#define _VMPROF_WRITE_ALL_CODE_OBJECTS_METHODDEF    \
    {"write_all_code_objects", (PyCFunction)_vmprof_write_all_code_objects, METH_NOARGS, _vmprof_write_all_code_objects__doc__},

static PyObject *
_vmprof_write_all_code_objects_impl(PyObject *module);

static PyObject *
_vmprof_write_all_code_objects(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return _vmprof_write_all_code_objects_impl(module);
}

PyDoc_STRVAR(_vmprof_sample_stack_now__doc__,
"sample_stack_now($module, /)\n"
"--\n"
"\n"
"Sample the current stack trace of the Python process.");

#define _VMPROF_SAMPLE_STACK_NOW_METHODDEF    \
    {"sample_stack_now", (PyCFunction)_vmprof_sample_stack_now, METH_NOARGS, _vmprof_sample_stack_now__doc__},

static PyObject *
_vmprof_sample_stack_now_impl(PyObject *module);

static PyObject *
_vmprof_sample_stack_now(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return _vmprof_sample_stack_now_impl(module);
}
/*[clinic end generated code: output=e51b167e777d8cdd input=a9049054013a1b77]*/
