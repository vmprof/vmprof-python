================
Viewing Profiles
================

vmprof writes a profile to a plain file on your machine, and that file is the
only thing the viewers need. Nothing is uploaded anywhere.

Record a profile
================

Pass ``-o`` to get a profile file::

    python -m vmprof -o profile.prof <program.py> <program arguments>

Add ``--lines`` if you also want per-line numbers inside functions::

    python -m vmprof --lines -o profile.prof <program.py> <program arguments>

From here, pick whichever of the viewers below suits the question you are
asking.

In the terminal: vmprofshow
===========================

``vmprofshow`` ships with vmprof and needs nothing else installed. It has
three modes.

``tree`` shows where time goes from the root of the call graph down::

    vmprofshow profile.prof tree

``--html`` writes the same tree as a page with branches you can expand and
collapse::

    vmprofshow profile.prof tree --html > profile.html

``flat`` aggregates per function instead, which is the view you want when a
function is called from many places and no single call site looks expensive::

    vmprofshow profile.prof flat

``lines`` shows the cost of individual lines, for profiles recorded with
``--lines``::

    vmprofshow profile.prof lines --filter <function-name>

Flame graphs and timelines: the Firefox Profiler
================================================

The `vmprof-firefox-converter`_ turns a profile into something the `Firefox
Profiler`_ UI can read, which gets you a flame graph, a stack chart over time,
an inverted call tree and a source view, all in the browser. It understands
PyPy's JIT frames too. See the `announcement post`_ for a tour with
screenshots.

Install it::

    python -m pip install vmprof-firefox-converter

Convert a profile you already recorded, which opens the Firefox Profiler on
it::

    python -m vmprofconvert -convert profile.prof

Add ``--nobrowser`` to only write the converted profile. You can also skip the
two steps and profile straight into the viewer::

    python -m vmprofconvert -run <program.py> <program arguments>

On PyPy you can fold the JIT and interpreter logs into the same view::

    PYPYLOG=profile.pypylog pypy -m vmprof --jitlog -o profile.prof <program.py>
    python -m vmprofconvert -convert profile.prof -jitlog profile.prof.jit -pypylog profile.pypylog

.. _`vmprof-firefox-converter`: https://github.com/Cskorpion/vmprof-firefox-converter
.. _`Firefox Profiler`: https://profiler.firefox.com
.. _`announcement post`: https://pypy.org/posts/2024/05/vmprof-firefox-converter.html

Call graphs: kcachegrind
========================

``vmprofshow`` can write a profile in `callgrind`_ format, which `kcachegrind`_
(or ``qcachegrind`` on Mac OS X and Windows) reads. That gives you the callee
and caller lists, the call graph view and sorting by self or inclusive cost::

    vmprofshow profile.prof callgrind -o profile.callgrind
    kcachegrind profile.callgrind

Without ``-o`` the callgrind data goes to stdout. The same file also works
with ``callgrind_annotate``, if you would rather stay in a terminal::

    vmprofshow profile.prof callgrind -o profile.callgrind
    callgrind_annotate profile.callgrind

Three things about this export are worth knowing, because they follow from
vmprof being a sampling profiler:

* The event is called ``Periods``, not instruction or cycle counts. Each
  sample is weighted by the time since the previous one, in units of the
  sampling period, so a cost is proportional to time spent rather than to
  the number of signals that happened to be delivered. Multiply by the
  period to get time: with the default ~1kHz, one unit is about 0.99ms, so
  a cost of 1000 is roughly a second.

* Call counts are not measured. vmprof never observes an individual call, so
  every edge is written as ``calls=1``. Read kcachegrind's call count column
  as "this call was seen", and ignore the numbers in it.

* Self cost is attributed to the line a function is defined on, since the
  profile does not record which line each call was made from. kcachegrind's
  source annotation will therefore put a function's whole cost on its ``def``
  line. Use ``vmprofshow profile.prof lines`` when you need line level detail.

Functions reached by more than one path are folded together, so a function
appears once with its costs summed, and recursion shows up as a cycle that
kcachegrind detects on its own.

.. _`callgrind`: https://valgrind.org/docs/manual/cl-format.html
.. _`kcachegrind`: https://kcachegrind.github.io/
