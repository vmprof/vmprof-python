=================
JIT Compiler Logs
=================

JitLog is a `PyPy`_ logging facility that outputs information about compiler internals.
It was built primarily for the following use cases:

* Understand JIT internals and be able to browse emitted code
* Track down speed issues
* Help with bug reporting

This version is now integrated within the webservice `vmprof.com`_ and can be used free
of charge.

Usage
=====

The following commands show example usages::

    # upload both vmprof & jitlog profiles
    pypy -m vmprof --web --jitlog <program.py> <arguments>

    # upload only a jitlog profile
    pypy -m jitlog --web <program.py> <arguments>

    # upload a jitlog when your program segfaults/crashes
    $ pypy -m jitlog -o /tmp/file.log <program.py> <arguments>
    <Segfault>
    $ pypy -m jitlog.upload /tmp/file.log

Data sent
=========

We only send the bare essentials to `vmprof.com`_. But to be able to reason
about your program we need small hot snippets of your program.

This includes:

* Meta data the JIT compiler produces. E.g. IR operations, Machine code
* Source code snippets. `vmprof.com`_ will receive

source lines of your program. Only those are transmitted that ran often enough
to trigger the JIT compiler to optimize your program.


.. _`vmprof.com`: http://vmprof.com
.. _`PyPy`: http://pypy.org
