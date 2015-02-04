
====================
vmprof documentation
====================

Introduction
============

vmprof is a lightweight statistical profiler for virtual machines that
records the stack content. Right now it has built-in support for `CPython 2.7`_
and `PyPy`_, currently only on ``vmprof`` branch. It fully supports PyPy
JIT features and while currently only with a simple command line interface,
we're working towards adding support for a web frontend as well as options
to agreggate multiple runs.

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

Installation
============

vmprof is a little tricky to install, you need some headers for various
libraries, the line on a recent Ubuntu is::

    apt-get install python-dev libdwarf-dev libelfg0-dev libunwind8-dev

and then::

    pip install vmprof

should work correctly on both PyPy (built from ``vmprof`` branch) or
CPython.

Usage
=====

The basic usage is ``python -m vmprof <your program> <your program params>``
for now. There is also an API that can bring you more details to the table,
but consider it unstable. The current API usage is as follows::

Module level functions
----------------------

* ``vmprof.enable(fileno, timeout=-1)`` - enable writing ``vmprof`` data to
  file described by a fileno file descriptor. Timeout is in microseconds, but
  the only available resolution is 4ms, we're working on improving that
  (default being 4ms)

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
