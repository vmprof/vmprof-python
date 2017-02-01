Native profiling
================

Version 0.4+ is able to profile native functions (routines written in
a different language like C) on Mac OS X and Linux. See below for a technical overview.

By default this feature is enabled. To disable native profiling add ``--no-native``
as a command line switch.

In Program Activation
---------------------

Whenever vmprof is enabled during the execution of a specific program part,
sometimes it will not sample the python level frames that have already
been entered. See the following example::

    # myprogram.py
    ...
    def func():
        vmporf.enable(fileno, ..., native=True)
        another_func()
        vmprof.disable()

If vmprof has never been activated before, the frames just before ``another_func()`` will
not be recorded.

The technical reason for this behaviour is: The
frame evaulation function is patched (e.g. it adds a trampoline to
PyEval_EvalFrameEx on CPython 2.7). All frames that have been activated to that
point, will not have a trampoline inserted, and thus will be ignored by the stack
sampling. To overcome this issue, invoke the whole program from command line::

    $ python -m vmprof ...

Technical Design
----------------

Native sampling utilizes ``libunwind`` in the signal handler to unwind the stack.

To find the corresponding ``PyFrameObject`` during stack unwinding vmprof inserts a trampoline on CPython (called ``vmprof_eval``) and places it just before ``PyEval_EvalFrameEx``. It is a callee trampoline saving the ``PyFrameObject`` in the callee saved register ``%rbx``. On Python 3.6+ the frame evaluation `PEP 523`_ is utilized as trampoline.

.. _`PEP 523`: https://www.python.org/dev/peps/pep-0523/

Symbols that are exposed from CPython/PyPy internally are ignored and not contained in the resulting profile. During stack sampling, instruction pointers that point into the virtual memory region of CPython/PyPy are not recorded.

