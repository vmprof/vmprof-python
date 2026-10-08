Develop VMProf
==============

vmprof is made up of a Python package and a C extension, built with
`meson-python`_. The `PyPy`_ side of the JIT log support lives in PyPy itself.

.. _`meson-python`: https://mesonbuild.com/meson-python/
.. _`PyPy`: http://pypy.org
.. _`vmprof-python`: https://github.com/vmprof/vmprof-python

Setting up
----------

Create a virtual environment and install vmprof in editable mode::

    $ git clone git@github.com:vmprof/vmprof-python.git
    $ cd vmprof-python
    $ python3 -m venv vmprof3
    $ source vmprof3/bin/activate
    $ pip install meson-python meson ninja
    $ pip install --no-build-isolation --editable .

Because the build is ``--no-build-isolation``, the C extension is rebuilt on
import when you change anything under ``src/``, so there is no separate build
step while developing.

You need your distribution's Python development headers, and on Linux the
libunwind headers as well. On Debian or Ubuntu those are ``python3-dev`` and
``libunwind-dev``.

Running the tests
-----------------

::

    $ pip install pytest cffi setuptools
    $ python -m pytest vmprof/

Some tests build small C extensions to exercise native profiling, which is why
``cffi`` and a compiler are needed.

Smaller profiles
----------------

Reading a profile of a long run is tedious while working on a feature. The
``vmprof/test/`` directory holds small recorded profiles, and
``vmprof/test/cpuburn.py`` generates fresh ones::

    $ python -m vmprof -o profile.prof vmprof/test/cpuburn.py

Working on the output modes
---------------------------

The viewers in :doc:`viewers` all read the same profile file, so a profile
recorded once can be replayed through every mode while you iterate::

    $ vmprofshow profile.prof tree
    $ vmprofshow profile.prof flat
    $ vmprofshow profile.prof callgrind -o profile.callgrind

The printers live in ``vmprof/show.py``. Each one subclasses
``AbstractPrinter`` and implements ``_show(tree)``, where ``tree`` is the
``Node`` tree built by ``Stats.get_tree()``; see ``vmprof/stats.py`` for what a
node carries. ``vmprof/test/test_show.py`` builds ``Node`` trees by hand, which
is the quickest way to test a new output mode without recording anything.
