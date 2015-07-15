
====================
vmprof documentation
====================

Introduction
============

`vmprof`_ is a lightweight profiler for `CPython`_ 2.7, 3, `PyPy`_ and other
virtual machines in the future. It helps you understand the performance
bottlenecks in your code.

vmprof is a `statistical profiler`_ - it gathers information about your
code by repeatedly getting the traceback in small intervals. This is similar
to tools like `vtune`_ or `gperftools`_, except it works for high-level virtual
machines rather than on the C level.

There are three primary modes. The most obvious one is to use our server
infrastructure for visualizations, then you do::


    python -m vmprof --web <program.py> <program parameters>

The more barebone one is::

    python -m vmprof <program.py> <program parameters>

which will display you only the statistical basics, or::

    python -m vmprof -o output.log <program.py> <program parameters>
    vmprofshow output.log

Which will display you a tree.

vmprof can be invoked and controlled with the API for more advanced use cases.

.. _`vmprof`: https://github.com/vmprof/vmprof-python
.. _`gperftools`:  https://code.google.com/p/gperftools/
.. _`vtune`: https://software.intel.com/en-us/intel-vtune-amplifier-xe
.. _`statistical profiler`: https://en.wikipedia.org/wiki/Profiling_(computer_programming)#Statistical_profilers

Requirements
------------

vmprof (as of 0.1) works on 64bit x86 linux only with beta support
of Mac OS X and Free BSD. It is supported on 
`CPython`_ 2.7, 3 and a recent `PyPy`_, at least 2.6.

Windows and 32bit support is planned.

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

But we stronly suggest using the ``--web`` option that will display you
a much nicer web interface hosted on ``vmprof.baroquesoftware.com``.

Options that follow ``-m vmprof`` are:

* ``--web`` - to be used together with `vmprof-server`_, defaults to
  ``vmprof.baroquesoftware.com`` as URL, uploads the output to the server as
  JSON. Can be viewed on the `server`_.

* ``--web-url`` - customize the URL for personal server

* ``--web-auth`` - auth token for user name support in the server

* ``-p period`` - float that gives you how often the profiling happens
  (the max is about 300 Hz, rather don't touch it)

* ``-n`` - enable all C frames, only useful if you have a debug build of
  pypy or cpython

* ``-o file`` - save logs for later

* ``--help`` - display help

.. _`vmprof-server`: https://github.com/vmprof/vmprof-server
.. _`server`: http://vmprof.baroquesoftware.com

There is also an API that can bring more details to the table,
but consider it unstable. The current API usage is as follows::

Module level functions
----------------------

* ``vmprof.enable(fileno, period=0.01)`` - enable writing ``vmprof`` data to a
  file described by a fileno file descriptor. Timeout is in float seconds. The
  minimal available resolution is 4ms, we're working on improving that
  (note the default is 10ms)

* ``vmprof.disable()`` - finish writing vmprof data, disable the signal handler

* ``vmprof.read_profile(filename, virtual_only=True)`` - read vmprof data
  from ``filename`` and return ``Stats`` instance. If ``virtual_only`` is set
  to ``False`` also report the C level stack (only if you know what you're
  doing, right now will report PyPy JIT code without aligning it properly,
  you've been warned)

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

There are a variety of python profilers on the market. `CProfile`_ is the one bundled
with CPython, together with `lsprofcalltree.py`_ it provides decent
visualization, while `plop`_ is an example of statistical profiler.

We want a few things when using a profiler:

* Minimal overhead, small enough to run it in production. 1-5%, ideally,
  with a possibility to tune it for more accurate measurments

* An ability to display a full stack of calls, so it can show how much time
  was spent in a function, including all its children

* Work under PyPy and be aware of the underlying JIT architecture to be
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
and special support for PyPy gives the same effect of being able to retrieve
Python stack from the C stack. This gives us a unique opportunity of being
able to see where is the JIT code, where is the Python code, what are we
doing in the C standard library (e.g. filter out the places where we are
inside ``select()`` calls, etc.). The machinery is there to report this 
information, we are working
on the frontend to make sure we can process and display the information.

