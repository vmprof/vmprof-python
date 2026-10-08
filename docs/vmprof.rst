============
CPU Profiles
============

vmprof is a `statistical profiler`_: it gathers information about your code by
continuously taking samples of the call stack of the running program, at a
given frequency. This is similar to tools like `vtune`_ or `gperftools`_: the
main difference is that those tools target C and C-like languages and are not
very helpful to profile higher-level languages which run on top of a virtual
machine, while vmprof is designed specifically for them. vmprof is also thread
safe and will correctly display the information regardless of usage of threads.

Profiling a program writes a profile file, which you then open in a viewer.
To get a quick overview in the terminal::

    python -m vmprof <program.py> <program parameters>

To keep the profile around, which is what you want for every viewer other
than the one above::

    python -m vmprof -o profile.prof <program.py> <program parameters>

That file can be read by the bundled ``vmprofshow`` command, by the Firefox
Profiler and by kcachegrind. :doc:`viewers` covers all three.

For more advanced use cases, vmprof can be invoked and controlled from within
the program using the given API.

.. _`vmprof`: https://github.com/vmprof/vmprof-python
.. _`gperftools`:  https://code.google.com/p/gperftools/
.. _`vtune`: https://software.intel.com/en-us/intel-vtune-amplifier-xe
.. _`statistical profiler`: https://en.wikipedia.org/wiki/Profiling_(computer_programming)#Statistical_profilers

Requirements
------------

vmprof 0.6 supports CPython 3.10 through 3.14 and PyPy, on Linux, Mac OS X and
Windows. Native profiling is available on Linux and Mac OS X.

Installation
------------

Installation of ``vmprof`` is performed with a simple command::

    pip install vmprof

PyPI ships wheels for Linux, Mac OS X and Windows, with the libunwind shared
objects bundled in. If you build from source you need the CPython development
headers, and on Linux the libunwind headers as well. On Debian or Ubuntu those
are the ``python3-dev`` and ``libunwind-dev`` packages. On Windows you need the
Microsoft Visual C++ compiler for your Python version.

Command line options
--------------------

After ``-m vmprof`` you can specify some options:

* ``-o file`` - save the profile to a file, to open in a viewer later.

* ``-p period`` - seconds between profile runs, sets the profiling frequency.
  The value must be between 1e-6 and 1.0, and should not result in a round
  number when converted to Hz to avoid aliasing problems. Default is 0.00099
  which results in ~1kHz.

* ``-n`` - enable all C frames, only useful if you have a debug build of
  PyPy or CPython.

* ``-m`` - indicate that the provided program should be executed as a module (like ``python -m``). Example: ``-m vmprof -m http.server``

* ``--lines`` - enable line profiling mode. This mode adds some overhead to profiling, but in addition to function calls it marks the execution of the specific lines inside functions.

* ``--mem`` - also record the total RSS of the process alongside the stacks.

* ``--jitlog`` - on PyPy, also write the JIT compiler log. See :doc:`jitlog`.

* ``--help`` - display help
  
* ``--config`` - a ini format config file with all options presented above. When passing a config file along with command line arguments, the command line arguments will take precedence and override the config file values.

Example `config.ini` file::

  [global]
  period = 0.0099
  lines = True


API
===

There is also an API that can bring more details to the table,
but consider it unstable. The current API usage is as follows:

Module level functions
----------------------

* ``vmprof.enable(fileno, period=0.00099, memory=False)`` - enable writing ``vmprof`` data to a
  file described by a fileno file descriptor. Period is in float seconds. The
  minimal available resolution is around 1ms, we're working on improving that
  (note the default is 0.99ms). Passing ``memory=True`` will provide additional
  data in the form of total RSS of the process memory interspersed with
  tracebacks.

* ``vmprof.disable()`` - finish writing vmprof data, disable the signal handler

* ``vmprof.read_profile(filename)`` - read vmprof data from
  ``filename`` and return ``Stats`` instance.

  ``start/stop_sampling()`` - Disables or starts the sampling of vmprof. This
  is useful to remove certain program parts from the profile. Be aware that
  those program parts still can be in the profile if that code is reached
  from another point in your program. In addition note that unix and windows
  implementation behave differntly. Unix will increment/decrement a counter,
  whereas windows has only two states for the counter (0 and 1).
  This may change in future.

``Stats`` object
----------------

Stats object gives you an overview of data:

* ``stats.get_tree()`` - Gives you a tree of objects

``Tree`` object
---------------

Tree is made of Nodes, each node supports at least the following interface:

* ``node[key]`` - a fuzzy search of keys (first match)

* ``repr(node)`` - basic details

* ``node.flatten()`` - returns a new tree that flattens all the metadata
  (gc, blackhole etc.)

* ``node.walk(callback)`` - call a callable of form ``callback(root)`` that will
  be invoked on each node

Why a new profiler?
===================

There is a variety of python profilers on the market: `CProfile`_ is the one
bundled with CPython, which together with `lsprofcalltree.py`_ provides good
info and decent visualization; `plop`_ is an example of statistical profiler.

We wanted a profiler with the following characteristics:

* Minimal overhead, small enough that enabling the profiler in production is a
  viable option. Ideally the overhead should be in the range 1-5%, with the
  possibility to tune it for more accurate measurments

* Ability to display a full stack of calls, so it can show how much time was
  spent in a function, including all its children

* Good integration with PyPy: in particular, it must be aware of the
  underlying JIT, and be able to show how much time is spent inside JITted
  code, Garbage collector and normal intepretation.

None of the existing solutions satisfied our requirements, hence we decided to
create our own profiler. In particular, cProfile is slow on PyPy, does not
understand the JITted code very well and is shown in the JIT traces.

.. _`CProfile`: https://docs.python.org/3/library/profile.html
.. _`lsprofcalltree.py`: https://pypi.python.org/pypi/lsprofcalltree
.. _`plop`: https://github.com/bdarnell/plop

How does it work?
=================

As most statistical profilers, the core idea is to have a signal handler which
periodically inspects and dumps the stack of the running program: the most
frequently executed parts of the code will be dumped more often, and the
post-processing and visualization tools have the chance to show the end user
useful info about the behavior of the profiled program. This is the very same
approach used e.g. by `gperftools`_.

However, when profiling an interpreter such as CPython, inspecting the C stack
is not enough, because most of the time will always be spent inside the opcode
dispatching loop of the virtual machine (e.g., ``_PyEval_EvalFrameDefault`` in
case of CPython).  To be able to display useful information, we need to know
which Python-level function correspond to each C-level frame evaluation.

This is done by reading the stack of Python frames instead of C stack.

Additionally, when on top of PyPy the C stack contains also stack frames which
belong to the JITted code: the vmprof signal handler is able to recognize and
extract the relevant info from those as well.

Once we have gathered all the low-level info, we can post-process and
visualize them in various ways: for example, we can decide to filter out the
places where we are inside the ``select()`` syscall, etc.
