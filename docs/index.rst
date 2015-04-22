
====================
vmprof documentation
====================

Introduction
============

vmprof is a lightweight profiler for `CPython`_ 2.7, `PyPy`_ and other
virtual machines in the future that helps you understand the performance
bottlenecks in your code.

vmprof works as a statistical profiler - it gathers information about your
code by repeatedly getting the traceback in small intervals. This is similar
to tools like `vtune`_ or `gperftools`_, except it works for high-level virtual
machines except o nthe C level.

The primary mode of running vmprof is through the module invocation with the
API for more advanced use cases.

.. _`gperftools`:  https://code.google.com/p/gperftools/
.. _`vtune`: https://software.intel.com/en-us/intel-vtune-amplifier-xe

Requirements
------------

vmprof as of 0.1 works on 64bit x86 linux for CPython 2.7 and a recent PyPy. We
hope to get PyPy 2.6 out of the door so it can be used with an official
release relatively soon.

OS X, Windows and 32bit support is planned.

Installation
------------

Installation of ``vmprof`` is performed with a simple command::

    pip install vmprof

You need a few packages. On ubuntu those are::

    sudo apt-get install python-dev libdwarf-dev libelfg0-dev libunwind8-dev

Usage
-----

Main usage of vmprof is via command line. Basic usage would look like that:

Example of usage::

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

The basic usage is ``python -m vmprof <your program> <your program params>``
for now.

Options that follow ``-m vmprof`` are:

* ``--web URL`` - to be used together with `vmprof-server`_ or use
  ``vmprof.baroquesoftware.com`` as URL, uploads the output to the server as
  JSON. Can be viewed on the `demo server`_.

* ``-o file`` - save logs for later

* ``--help`` - display help

.. _`vmprof-server`: https://github.com/vmprof/vmprof-server
.. _`demo server`: http://vmprof.baroquesoftware.com

There is also an API that can bring you more details to the table,
but consider it unstable. The current API usage is as follows::

Module level functions
----------------------

* ``vmprof.enable(fileno, period=0.01)`` - enable writing ``vmprof`` data to
  file described by a fileno file descriptor. Timeout is in float seconds, but
  the only available resolution is 4ms, we're working on improving that
  (default being 10ms)

* ``vmprof.disable()`` - finish writing vmprof data, disable the signal handler

* ``vmprof.read_profile(filename, virtual_only=True)`` - read vmprof data
  from ``filename`` and return ``Stats`` instance. If ``virtual_only`` is set
  to ``False`` also report the C level stack (only if you know what you're
  doing, right now will report PyPy JIT code without aligning it properly,
  you've been warned)

``Stats`` object
----------------

Stats object gives you an overview of data:

* ``stats.top_profile()`` - list of (unsorted) tuples ``name`` of a format
  ``py:func_name:startlineno:filename`` and number of profiler samples recorded

* ``stats.adr_dict`` - a dictionary of ``address`` -> ``name`` for Python
  functions.

* ``stats._get_name(addr)`` - gives you a ``name`` for ``address``

* ``stats.functions`` - similar to ``stats.top_profile()`` but does not
  do name lookup and instead returns you python function addresses

* ``stats.function_profile(function_addr)`` - generate a (sorted) profile
  data for function given by ``function_addr``, so all the functions called
  by this function

Why a new profiler?
===================

There is a variety of options on the market. `CProfile`_ is the one bundled
with CPython, together with `lsprofcalltree.py`_ it provides decent
visualization, while `plop`_ is an example of statistical profiler.

We want a few things when using a profiler:

* Minimal overhead, small enough to run it in production. 1-5%, ideally,
  with a possibility to tune it for more accurate measurments

* An ability to display a full stack of calls, so it can show how much time
  got spent in a function, including all its children

* Work under PyPy and be aware of the underlaying JIT architecture to be
  able to show jitted/not jitted code

So far none of the existing solutions satisfied our requirements, hence
we decided to create our own profiler. Notably cProfile is slow on PyPy,
does not understand the JITted code very well and shows in the JIT traces.

.. _`CProfile`: https://docs.python.org/2/library/profile.html
.. _`lsprofcalltree.py`: https://pypi.python.org/pypi/lsprofcalltree
.. _`plop`: https://github.com/bdarnell/plop

How does it work?
=================

The main work is done by a signal handler that inspects the C stack (very
much like gperftools). Additionally there is a special trampoline for CPython
and a special support for PyPy gives the same effect of being able to retrieve
Python stack from the C stack. This gives us a unique opportunity of being
able to look where is the JIT code, where is the Python code, what are we
doing in the C standard library (e.g. filter out the places where we are
inside the ``select()`` call etc.). The machinery is there, we are working
on the frontend to make sure we can process this information.

