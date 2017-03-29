Data sent to vmprof.com
=======================

We only send the bare essentials to `vmprof.com`_. This package is no spy software.

It includes the following data:

* The full command line
* The name of the interpreter used
* Filesystem path names, function names and line numbers of to your scripts
* Generic system information (Operating system, CPU word size, ...)

If jit log data is sent (--jitlog) on PyPy the following is also included:

* Meta data the JIT compiler produces. E.g. IR operations, Machine code
* Source code snippets: `vmprof.com`_ will receive source lines of your program. Only those are transmitted that ran often enough to trigger the JIT compiler to optimize your program.

.. _`vmprof.com`: http://vmprof.com
.. _`PyPy`: http://pypy.org
