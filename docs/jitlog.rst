=================
JIT Compiler Logs
=================

JitLog is a `PyPy`_ logging facility that outputs information about compiler internals.
It was built primarily for the following use cases:

* Understand JIT internals and be able to browse emitted code (both IR operations and machine code)
* Track down speed issues
* Help bug reporting

This version is now integrated within the webservice `vmprof.com`_ and can be used free of charge.

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
    $ pypy -m jitlog --upload /tmp/file.log

.. _`vmprof.com`: http://vmprof.com
.. _`PyPy`: http://pypy.org
