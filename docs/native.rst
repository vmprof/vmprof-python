Native profiling
================

Version 0.4+ is able to profile native functions (routines written in
a different language like C) on Mac OS X and Linux. See below for a technical overview.

By default this feature is enabled. To disable native profiling add ``--no-native``
as a command line switch.

**NOTE** be sure to provide debugging symbols for your native functions, otherwise
you will not see the symbol name of your e.g. C program.

Technical Design
----------------

Native sampling utilizes ``libunwind`` in the signal handler to unwind the stack.

Each stack frame is inspected until the frame evaluation function is encountered. Then the stack walking
switches back to the traditional Python frame walking. Callbacks (Python frame -> ... C frame ... -> Python frame ->
 C frame)
will not display intermediate native functions. It would give the impression that the first C frame was never called,
but it will show the second C frame.

Earlier Implementation
----------------------

Prior to 0.4.3 the following logic was implemented (see e.g. commit 3912330b509d).
It was removed because it could not be implemented on Mac OS X
(libunwind misses register/cancel functions for generated machine code).

To find the corresponding ``PyFrameObject`` during stack unwinding vmprof inserts a trampoline on CPython (called ``vmprof_eval``) and places it just before ``PyEval_EvalFrameEx``. It is a callee trampoline saving the ``PyFrameObject`` in the callee saved register ``%rbx``. On Python 3.6+ the frame evaluation `PEP 523`_ is utilized as trampoline.

.. _`PEP 523`: https://www.python.org/dev/peps/pep-0523/

