=================
JIT Compiler Logs
=================

JitLog is a `PyPy`_ logging facility that outputs information about compiler internals.
It was built primarily for the following use cases:

* Understand JIT internals and be able to browse emitted code (both IR operations and machine code)
* Track down speed issues
* Help bug reporting

Usage
=====

Recording a JIT log alongside a CPU profile writes it next to the profile,
with a ``.jit`` suffix::

    pypy -m vmprof --jitlog -o profile.prof <program.py> <arguments>
    # writes profile.prof and profile.prof.jit

To record only the JIT log, without profiling::

    pypy -m jitlog -o profile.jit <program.py> <arguments>

This also works when your program crashes, since the log is written as it
goes: run it, let it segfault, and the log is still there to inspect.

Viewing a JIT log
=================

The `vmprof-firefox-converter`_ can fold a JIT log, and PyPy's own log, into
the same Firefox Profiler view as the CPU profile::

    PYPYLOG=profile.pypylog pypy -m vmprof --jitlog -o profile.prof <program.py>
    python -m vmprofconvert -convert profile.prof -jitlog profile.prof.jit -pypylog profile.pypylog

To read traces in the terminal, use the query interface described in
:doc:`query`::

    pypy -m jitlog profile.jit -q 'bridges & op("int_add_ovf")'

.. _`vmprof-firefox-converter`: https://github.com/Cskorpion/vmprof-firefox-converter
.. _`PyPy`: http://pypy.org
