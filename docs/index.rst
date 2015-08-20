
====================
vmprof documentation
====================

Introduction
============

`vmprof`_ is a lightweight profiler for `CPython`_ 2.7, `CPython`_ 3, `PyPy`_,
and possibly even other non-Python virtual machines in the future. It helps
you to find and understand the performance bottlenecks in your code.

vmprof is a `statistical profiler`_: it gathers information about your code by
continuously taking samples of the C call stack of the running program, at a
given frequency. This is similar to tools like `vtune`_ or `gperftools`_: the
main difference is that those tools target C and C-like languages and are not
very helpful to profile higher-level languages which run on top of a virtual
machine, while vmprof is designed specifically for them.

There are three primary modes. The recommended one is to use our server
infrastructure for a web-based visualization of the result::

    python -m vmprof --web <program.py> <program parameters>

If you prefer a barebone terminal-based visualization, which will display only
some basic statistics::

    python -m vmprof <program.py> <program parameters>

To display a terminal-based tree of calls::

    python -m vmprof -o output.log <program.py> <program parameters>

    vmprofshow output.log

To upload an already saved profile log to the vmprof web server::

    python -m vmprof.upload output.log

For more advanced use cases, vmprof can be invoked and controlled from within
the program using the given API.

.. _`vmprof`: https://github.com/vmprof/vmprof-python
.. _`gperftools`:  https://code.google.com/p/gperftools/
.. _`vtune`: https://software.intel.com/en-us/intel-vtune-amplifier-xe
.. _`statistical profiler`: https://en.wikipedia.org/wiki/Profiling_(computer_programming)#Statistical_profilers

Requirements
------------

vmprof 0.1 works only on x86_64 linux, with upcoming support of Mac OS X and
Free BSD. MacOS X and Free BSD should work with PyPy trunk (but not 2.6)
It supports  `CPython`_ 2.7, `CPython`_ 3 and `PyPy`_ >= 2.6.

Currently it does not work on Windows and 32bit machines, although support for
those is planned in the future.

Installation
------------

Installation of ``vmprof`` is performed with a simple command::

    pip install vmprof

Since it depends on some C code and external libraries, you need a compiler
and some packages. On ubuntu those are::

    sudo apt-get install python-dev libdwarf-dev libelfg0-dev libunwind8-dev

Usage
-----

Main usage of vmprof is via command line. The following shows the basic usage
to profile an example ``x.py`` program::

  fijal@hermann:~/src/vmprof-python$ cat x.py
  
  def g(i, s):
      s += i
      return s
  
  def h(i, s):
      return g(i, s) + 3
  
  def f():
      i = 0
      s = 0
      while i < 10000000:
          s = h(i, s)
          i += 1

  if __name__ == '__main__':
      f()

  fijal@hermann:~/src/vmprof-python$ python -m vmprof x.py
  vmprof output:
  % of snapshots:  name:
   100.0%          <module>    x.py:2
   100.0%          f    x.py:9
   55.0%           h    x.py:6
   14.4%           g    x.py:2

.. _`CPython`: http://python.org
.. _`PyPy`: http://pypy.org

We stronly suggest using the ``--web`` option that will display you a much
nicer web interface hosted on ``vmprof.baroquesoftware.com``.

If you prefer to host your own vmprof visualization server, you need the
`vmprof-server` package.

After ``-m vmprof`` you can specify some options:

* ``--web`` - Use the web-based visualization. By default, the result can be
  viewed on our `server`_.

* ``--web-url`` - the URL to upload the profiling info as JSON. The default is
  ``vmprof.baroquesoftware.com``

* ``--web-auth`` - auth token for user name support in the server.

* ``-p period`` - float that gives you how often the profiling happens
  (the max is about 300 Hz, rather don't touch it).

* ``-n`` - enable all C frames, only useful if you have a debug build of
  PyPy or CPython.

* ``-o file`` - save logs for later

* ``--help`` - display help
  
* ``--config`` - a ini format config file with all options presented above. When passing a config file along with command line arguments, the command line arguments will take precedence and override the config file values.

Example `config.ini` file::

  web-url = vmprof.baroquesoftware.com
  web-auth = ffb7d4bee2d6436bbe97e4d191bf7d23f85dfeb2
  period = 0.1

.. _`vmprof-server`: https://github.com/vmprof/vmprof-server
.. _`server`: http://vmprof.baroquesoftware.com


API
===


There is also an API that can bring more details to the table,
but consider it unstable. The current API usage is as follows::

Module level functions
----------------------

* ``vmprof.enable(fileno, period=0.01)`` - enable writing ``vmprof`` data to a
  file described by a fileno file descriptor. Timeout is in float seconds. The
  minimal available resolution is 4ms, we're working on improving that
  (note the default is 10ms)

* ``vmprof.disable()`` - finish writing vmprof data, disable the signal handler

* ``vmprof.read_profile(filename, virtual_only=True)`` - read vmprof data from
  ``filename`` and return ``Stats`` instance. If ``virtual_only`` is set to
  ``False`` also report the C level stack (use it only if you know what you're
  doing. Right now it will report the PyPy JIT code without aligning it
  properly, you've been warned)

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

.. _`CProfile`: https://docs.python.org/2/library/profile.html
.. _`lsprofcalltree.py`: https://pypi.python.org/pypi/lsprofcalltree
.. _`plop`: https://github.com/bdarnell/plop

How does it work?
=================

As most statistical profilers, the core idea is to have a signal handler which
periodically inspects and dumps the C stack of the running program: the most
frequently executed parts of the code will be dumped more often, and the
post-processing and visualization tools have the chance to show the end user
usueful info about the behavior of the profiled program. This is the very same
approach used e.g. by `gperftools`_.

However, when profiling an interpreter such as CPython, inspecting the C stack
is not enough, because most of the time will always be spent inside the opcode
dispatching loop of the virtual machine (e.g., ``PyEval_EvalFrameEx`` in case
of CPython).  To be able to display useful information, we need to know which
Python-level function correspond to each C-level ``PyEval_EvalFrameEx``.

This is done by using a special trampoline: vmprof's signal handler recognizes
the C-level stack frames of this trampoline, and fishes the corresponding
``PyFrame*`` object from the C stack, which is then used to gather all the
Python-level info it needs, such as the code object which is being executed.

Additionally, when on top of PyPy the C stack contains also stack frames which
belong to the JITted code: the vmprof signal handler is able to recognize and
extract the relevant info from those as well.

Once we have gathered all the low-level info, we can post-process and
visualize them in various ways: for example, we can decide to filter out the
places where we are inside the ``select()`` syscall, etc.

The machinery to gather the information has been the focus of the initial
phase of vmprof development and now it is working well: we are currently
focusing on the frontend to make sure we can process and display the info in
useful ways.
