
.. image:: _static/vmprof-logo.png
    :width: 386px 
    :align: center


|
|

vmprof
======

`vmprof`_ is a lightweight `statistical profiler`_ for `CPython`_ 3.10+ and
`PyPy`_, along with an assembler log reader for `PyPy`_. It runs on Linux,
Mac OS X and Windows.

Profiling writes a profile file, which you open in the viewer of your choice:
the bundled ``vmprofshow``, the Firefox Profiler, or kcachegrind::

    pip install vmprof
    python -m vmprof -o profile.prof <program.py> <program arguments>
    vmprofshow profile.prof tree

.. toctree::
   :maxdepth: 2

   vmprof
   viewers
   faq
   native
   format
   jitlog
   query
   development

.. _`CPython`: http://python.org
.. _`PyPy`: http://pypy.org
.. _`vmprof`: https://github.com/vmprof/vmprof-python
.. _`statistical profiler`: https://en.wikipedia.org/wiki/Profiling_(computer_programming)#Statistical_profilers
